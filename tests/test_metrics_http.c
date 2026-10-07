/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "metrics.h"
#include "stats.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void child_client(unsigned port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    assert(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
    assert(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);

    /* Reproduce the race seen with vmagent: TCP connects before HTTP bytes arrive. */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 150000000L };
    assert(nanosleep(&ts, NULL) == 0);

    const char req[] =
        "GET /metrics HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "User-Agent: vmagent/test\r\n"
        "Accept: text/plain\r\n"
        "Connection: close\r\n\r\n";
    assert(send(fd, req, sizeof(req) - 1, 0) == (ssize_t)(sizeof(req) - 1));

    char response[8192];
    size_t used = 0;
    for (;;) {
        ssize_t n = recv(fd, response + used, sizeof(response) - 1 - used, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        used += (size_t)n;
        if (used == sizeof(response) - 1) break;
    }
    response[used] = '\0';

    assert(strstr(response, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(response, "gm10_pulses_total 42") != NULL);
    assert(strstr(response, "gm10_max_events_1s 42") != NULL);
    assert(strstr(response, "gm10_serial_bytes_total 42") != NULL);
    assert(strstr(response, "gm10_serial_multibyte_bytes_total 41") != NULL);
    assert(strstr(response, "gm10_serial_max_read_size_bytes 33") != NULL);
    close(fd);
    _exit(0);
}

int main(void)
{
    struct gm10_metrics_server metrics;
    struct gm10_stats stats;
    unsigned port = 20000U + ((unsigned)getpid() % 20000U);

    gm10_metrics_init(&metrics);
    gm10_stats_init(&stats, 1000);
    gm10_stats_record_serial_read(&stats, 1);
    gm10_stats_record_serial_read(&stats, 3);
    gm10_stats_record_serial_read(&stats, 5);
    gm10_stats_record_serial_read(&stats, 33);
    gm10_stats_add(&stats, 1000, 42);
    assert(gm10_metrics_open(&metrics, "127.0.0.1", (uint16_t)port) == 0);

    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) child_client(port);

    struct pollfd pfd = { .fd = metrics.fd, .events = POLLIN, .revents = 0 };
    assert(poll(&pfd, 1, 2000) == 1);
    assert(pfd.revents & POLLIN);
    gm10_metrics_serve_one(&metrics, &stats, 1000, true, true, true);

    int status = 0;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);

    gm10_metrics_close(&metrics);
    puts("metrics HTTP race test: ok");
    return 0;
}
