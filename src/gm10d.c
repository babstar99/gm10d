/*
 * gm10d - Black Cat Systems GM-10 radiation detector daemon
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2026 gm10d contributors
 *
 * Hardware behaviour and definitions are based on gm4lin 1.2.15:
 * Copyright (C) 2003 Mathias Bavay
 */
#include "config.h"
#include "metrics.h"
#include "mqtt.h"
#include "serial.h"
#include "stats.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define GM10D_VERSION "0.1.1"

static volatile sig_atomic_t stop_requested = 0;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static uint64_t monotonic_sec(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) return 0;
    return (uint64_t)ts.tv_sec;
}

static void usage(FILE *out, const char *argv0)
{
    fprintf(out,
        "Usage: %s [-c FILE] [--version]\n"
        "  -c, --config FILE   config file (default /etc/gm10d.conf)\n"
        "  -V, --version       print version\n"
        "  -h, --help          show this help\n", argv0);
}

int main(int argc, char **argv)
{
    const char *config_path = "/etc/gm10d.conf";
    struct gm10_config cfg;
    struct gm10_serial serial;
    struct gm10_stats stats;
    struct gm10_metrics_server metrics;
    struct gm10_mqtt mqtt;
    uint64_t now, next_serial_retry = 0, next_mqtt_publish = 0;
    bool serial_ever_connected = false;
    bool mqtt_lib_ready = false;
    int rc = EXIT_FAILURE;

    for (int i = 1; i < argc; i++) {
        if ((!strcmp(argv[i], "-c") || !strcmp(argv[i], "--config")) && i + 1 < argc) {
            config_path = argv[++i];
        } else if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
            printf("gm10d %s\n", GM10D_VERSION);
            return EXIT_SUCCESS;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(stdout, argv[0]);
            return EXIT_SUCCESS;
        } else {
            usage(stderr, argv[0]);
            return EXIT_FAILURE;
        }
    }

    gm10_config_defaults(&cfg);
    if (gm10_config_load(&cfg, config_path) || gm10_config_validate(&cfg)) return EXIT_FAILURE;

    if (signal(SIGINT, on_signal) == SIG_ERR || signal(SIGTERM, on_signal) == SIG_ERR) {
        fprintf(stderr, "gm10d: cannot install signal handlers\n");
        return EXIT_FAILURE;
    }
    signal(SIGPIPE, SIG_IGN);

    gm10_serial_init(&serial);
    gm10_metrics_init(&metrics);
    gm10_mqtt_init(&mqtt);
    now = monotonic_sec();
    gm10_stats_init(&stats, now);

    if (gm10_metrics_open(&metrics, cfg.metrics_bind, cfg.metrics_port)) goto out;

    if (gm10_mqtt_global_init()) goto out;
    mqtt_lib_ready = true;
    if (gm10_mqtt_start(&mqtt, &cfg)) {
        fprintf(stderr, "gm10d: MQTT disabled after setup failure; serial acquisition continues\n");
    }

    fprintf(stderr, "gm10d: started version %s\n", GM10D_VERSION);

    while (!stop_requested) {
        struct pollfd fds[2];
        nfds_t nfds = 0;
        int serial_index = -1, metrics_index = -1;

        now = monotonic_sec();
        if (serial.fd < 0 && now >= next_serial_retry) {
            if (gm10_serial_open(&serial, cfg.device) == 0) {
                uint64_t discarded = 0;
                int settle_rc = gm10_serial_settle(&serial, cfg.startup_settle_seconds, &discarded);
                stats.startup_discarded_bytes_total += discarded;

                if (settle_rc == 0) {
                    if (serial_ever_connected) stats.reconnects_total++;
                    serial_ever_connected = true;
                    gm10_mqtt_set_detector_connected(&mqtt, true);
                } else {
                    fprintf(stderr,
                            "gm10d: detector startup settle failed; retrying in %u seconds\n",
                            cfg.serial_retry_seconds);
                    gm10_serial_close(&serial);
                    gm10_mqtt_set_detector_connected(&mqtt, false);
                    next_serial_retry = now + cfg.serial_retry_seconds;
                }
            } else {
                next_serial_retry = now + cfg.serial_retry_seconds;
            }
        }

        if (serial.fd >= 0) {
            serial_index = (int)nfds;
            fds[nfds].fd = serial.fd;
            fds[nfds].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
            fds[nfds].revents = 0;
            nfds++;
        }
        if (metrics.fd >= 0) {
            metrics_index = (int)nfds;
            fds[nfds].fd = metrics.fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        int prc = poll(fds, nfds, 1000);
        now = monotonic_sec();
        if (prc < 0 && errno != EINTR) {
            fprintf(stderr, "gm10d: poll: %s\n", strerror(errno));
            break;
        }

        if (serial_index >= 0 && fds[serial_index].revents) {
            short re = fds[serial_index].revents;
            if (re & POLLIN) {
                unsigned char buf[512];
                for (;;) {
                    ssize_t n = gm10_serial_read(&serial, buf, sizeof(buf));
                    if (n > 0) {
                        /* Black Cat protocol: each received byte represents one particle event. */
                        gm10_stats_add(&stats, now, (uint32_t)n);
                        continue;
                    }
                    if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                        if (n < 0) {
                            fprintf(stderr, "gm10d: serial read: %s\n", strerror(errno));
                            stats.read_errors_total++;
                            re |= POLLERR;
                        }
                    }
                    break;
                }
            }
            if (re & (POLLERR | POLLHUP | POLLNVAL)) {
                fprintf(stderr, "gm10d: serial disconnected; retrying in %u seconds\n", cfg.serial_retry_seconds);
                gm10_serial_close(&serial);
                gm10_mqtt_set_detector_connected(&mqtt, false);
                next_serial_retry = now + cfg.serial_retry_seconds;
            }
        }

        if (metrics_index >= 0 && (fds[metrics_index].revents & POLLIN)) {
            /* Drain a small burst of scrape connections without blocking acquisition. */
            for (int i = 0; i < 8; i++) {
                gm10_metrics_serve_one(&metrics, &stats, now,
                                       serial.fd >= 0, cfg.mqtt_enable,
                                       gm10_mqtt_is_connected(&mqtt));
            }
        }

        if (cfg.mqtt_enable && now >= next_mqtt_publish) {
            (void)gm10_mqtt_publish_state(&mqtt, &stats, now, serial.fd >= 0);
            next_mqtt_publish = now + cfg.mqtt_publish_interval;
        }
    }

    rc = EXIT_SUCCESS;

out:
    fprintf(stderr, "gm10d: stopping\n");
    gm10_mqtt_stop(&mqtt);
    if (mqtt_lib_ready) gm10_mqtt_global_cleanup();
    gm10_metrics_close(&metrics);
    gm10_serial_close(&serial);
    return rc;
}
