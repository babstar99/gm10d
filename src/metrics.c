/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "metrics.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

void gm10_metrics_init(struct gm10_metrics_server *m)
{
    m->fd = -1;
}

int gm10_metrics_open(struct gm10_metrics_server *m, const char *bind_addr, uint16_t port)
{
    int fd, one = 1, flags;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd == -1) {
        fprintf(stderr, "gm10d: metrics socket: %s\n", strerror(errno));
        return -1;
    }
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, bind_addr, &addr.sin_addr) != 1) {
        fprintf(stderr, "gm10d: invalid metrics_bind address '%s' (IPv4 literal required)\n", bind_addr);
        close(fd);
        return -1;
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        fprintf(stderr, "gm10d: metrics bind %s:%u: %s\n", bind_addr, port, strerror(errno));
        close(fd);
        return -1;
    }
    if (listen(fd, 8) == -1) {
        fprintf(stderr, "gm10d: metrics listen: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    m->fd = fd;
    fprintf(stderr, "gm10d: metrics listening on http://%s:%u/metrics\n", bind_addr, port);
    return 0;
}

void gm10_metrics_close(struct gm10_metrics_server *m)
{
    if (m->fd >= 0) close(m->fd);
    m->fd = -1;
}

static void send_all(int fd, const char *buf, size_t len)
{
    while (len) {
        ssize_t n = send(fd, buf, len, MSG_NOSIGNAL);
        if (n <= 0) return;
        buf += n;
        len -= (size_t)n;
    }
}

void gm10_metrics_serve_one(struct gm10_metrics_server *m,
                            const struct gm10_stats *stats,
                            uint64_t now_sec,
                            bool serial_connected,
                            bool mqtt_enabled,
                            bool mqtt_connected)
{
    int cfd;
    char req[512];
    char body[4096];
    char hdr[256];
    uint32_t cpm = gm10_stats_window(stats, now_sec, 60);
    uint32_t counts10 = gm10_stats_window(stats, now_sec, 10);
    double cps = cpm / 60.0;
    uint64_t uptime = now_sec >= stats->started_monotonic_sec ? now_sec - stats->started_monotonic_sec : 0;

    cfd = accept4(m->fd, NULL, NULL, SOCK_CLOEXEC);
    if (cfd == -1) return;
    (void)recv(cfd, req, sizeof(req) - 1, MSG_DONTWAIT);

    int n = snprintf(body, sizeof(body),
        "# HELP gm10_pulses_total Total particle events received from the GM-10 since process start.\n"
        "# TYPE gm10_pulses_total counter\n"
        "gm10_pulses_total %" PRIu64 "\n"
        "# HELP gm10_cpm Particle events received during the preceding 60 seconds.\n"
        "# TYPE gm10_cpm gauge\n"
        "gm10_cpm %u\n"
        "# HELP gm10_cps Counts per second derived from the rolling 60-second CPM.\n"
        "# TYPE gm10_cps gauge\n"
        "gm10_cps %.6f\n"
        "# HELP gm10_counts_10s Particle events received during the preceding 10 seconds.\n"
        "# TYPE gm10_counts_10s gauge\n"
        "gm10_counts_10s %u\n"
        "# HELP gm10_cpm_10s Ten-second count extrapolated to CPM, compatible with gm4lin activity mode.\n"
        "# TYPE gm10_cpm_10s gauge\n"
        "gm10_cpm_10s %u\n"
        "# HELP gm10_serial_connected Whether the serial detector is currently connected.\n"
        "# TYPE gm10_serial_connected gauge\n"
        "gm10_serial_connected %d\n"
        "# HELP gm10_mqtt_connected Whether MQTT is currently connected; zero when MQTT is disabled.\n"
        "# TYPE gm10_mqtt_connected gauge\n"
        "gm10_mqtt_connected %d\n"
        "# HELP gm10_serial_read_errors_total Serial read/HUP errors since process start.\n"
        "# TYPE gm10_serial_read_errors_total counter\n"
        "gm10_serial_read_errors_total %" PRIu64 "\n"
        "# HELP gm10_serial_reconnects_total Successful serial reconnections after startup.\n"
        "# TYPE gm10_serial_reconnects_total counter\n"
        "gm10_serial_reconnects_total %" PRIu64 "\n"
        "# HELP gm10_uptime_seconds gm10d process uptime in seconds.\n"
        "# TYPE gm10_uptime_seconds gauge\n"
        "gm10_uptime_seconds %" PRIu64 "\n",
        stats->pulses_total, cpm, cps, counts10, counts10 * 6U,
        serial_connected ? 1 : 0,
        mqtt_enabled && mqtt_connected ? 1 : 0,
        stats->read_errors_total, stats->reconnects_total, uptime);

    if (n < 0 || (size_t)n >= sizeof(body)) {
        close(cfd);
        return;
    }
    int h = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n", n);
    if (h > 0 && (size_t)h < sizeof(hdr)) {
        send_all(cfd, hdr, (size_t)h);
        send_all(cfd, body, (size_t)n);
    }
    close(cfd);
}
