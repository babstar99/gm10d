/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hardware behaviour follows gm4lin 1.2.15 by Mathias Bavay (2003):
 * 57600 baud, 8N1, receiver enabled, and DTR asserted to power the probe.
 */
#include "serial.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifndef CRTSCTS
#define CRTSCTS 0
#endif

void gm10_serial_init(struct gm10_serial *s)
{
    s->fd = -1;
}

static int set_dtr(int fd, int on)
{
    int bit = TIOCM_DTR;
    int request = on ? TIOCMBIS : TIOCMBIC;
    if (ioctl(fd, request, &bit) == -1) {
        fprintf(stderr, "gm10d: %s DTR failed: %s\n", on ? "assert" : "clear", strerror(errno));
        return -1;
    }
    return 0;
}

int gm10_serial_open(struct gm10_serial *s, const char *device)
{
    struct termios tty;
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd == -1) {
        fprintf(stderr, "gm10d: cannot open %s: %s\n", device, strerror(errno));
        return -1;
    }

    if (tcgetattr(fd, &tty) == -1) {
        fprintf(stderr, "gm10d: tcgetattr(%s): %s\n", device, strerror(errno));
        close(fd);
        return -1;
    }

    /* Start from raw mode, then explicitly reproduce gm4lin's 57600 8N1 setup. */
    cfmakeraw(&tty);
    if (cfsetispeed(&tty, B57600) == -1 || cfsetospeed(&tty, B57600) == -1) {
        fprintf(stderr, "gm10d: cannot set %s to 57600 baud: %s\n", device, strerror(errno));
        close(fd);
        return -1;
    }

    tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
    tty.c_cflag |= CS8 | CLOCAL | CREAD;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) == -1) {
        fprintf(stderr, "gm10d: tcsetattr(%s): %s\n", device, strerror(errno));
        close(fd);
        return -1;
    }

    if (tcflush(fd, TCIFLUSH) == -1) {
        fprintf(stderr, "gm10d: warning: tcflush(%s): %s\n", device, strerror(errno));
    }

    /* gm4lin explicitly asserts DTR to provide power to the GM-10/GM-45. */
    if (set_dtr(fd, 1) == -1) {
        close(fd);
        return -1;
    }

    s->fd = fd;
    fprintf(stderr, "gm10d: serial connected: %s (57600 8N1, DTR asserted)\n", device);
    return 0;
}


static uint64_t monotonic_msec(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) return 0;
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

int gm10_serial_settle(struct gm10_serial *s, unsigned seconds, uint64_t *discarded_bytes)
{
    unsigned char buf[512];
    uint64_t discarded = 0;
    uint64_t start_ms, deadline_ms;

    if (discarded_bytes) *discarded_bytes = 0;
    if (s->fd < 0) {
        errno = EBADF;
        return -1;
    }

    if (seconds == 0) {
        if (tcflush(s->fd, TCIFLUSH) == -1) {
            fprintf(stderr, "gm10d: startup input flush failed: %s\n", strerror(errno));
            return -1;
        }
        fprintf(stderr, "gm10d: detector startup settle disabled; input flushed\n");
        return 0;
    }

    fprintf(stderr,
            "gm10d: detector settling for %u second%s; discarding startup serial input\n",
            seconds, seconds == 1 ? "" : "s");

    start_ms = monotonic_msec();
    deadline_ms = start_ms + (uint64_t)seconds * 1000ULL;

    for (;;) {
        struct pollfd pfd;
        uint64_t now_ms = monotonic_msec();
        int timeout_ms;
        int prc;

        if (now_ms >= deadline_ms) break;
        timeout_ms = (int)(deadline_ms - now_ms);

        pfd.fd = s->fd;
        pfd.events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
        pfd.revents = 0;
        prc = poll(&pfd, 1, timeout_ms);
        if (prc == -1) {
            if (errno == EINTR) continue;
            fprintf(stderr, "gm10d: startup settle poll failed: %s\n", strerror(errno));
            if (discarded_bytes) *discarded_bytes = discarded;
            return -1;
        }
        if (prc == 0) break;
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "gm10d: serial error during detector startup settle\n");
            if (discarded_bytes) *discarded_bytes = discarded;
            errno = EIO;
            return -1;
        }
        if (pfd.revents & POLLIN) {
            for (;;) {
                ssize_t n = read(s->fd, buf, sizeof(buf));
                if (n > 0) {
                    discarded += (uint64_t)n;
                    continue;
                }
                if (n == -1 && errno == EINTR) continue;
                if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    fprintf(stderr, "gm10d: startup settle read failed: %s\n", strerror(errno));
                    if (discarded_bytes) *discarded_bytes = discarded;
                    return -1;
                }
                break;
            }
        }
    }

    /* Drain anything that arrived at the boundary before acquisition begins. */
    for (;;) {
        ssize_t n = read(s->fd, buf, sizeof(buf));
        if (n > 0) {
            discarded += (uint64_t)n;
            continue;
        }
        if (n == -1 && errno == EINTR) continue;
        if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "gm10d: final startup drain failed: %s\n", strerror(errno));
            if (discarded_bytes) *discarded_bytes = discarded;
            return -1;
        }
        break;
    }

    if (discarded_bytes) *discarded_bytes = discarded;
    fprintf(stderr, "gm10d: detector ready; discarded %llu startup byte%s\n",
            (unsigned long long)discarded, discarded == 1 ? "" : "s");
    return 0;
}

ssize_t gm10_serial_read(struct gm10_serial *s, void *buf, size_t len)
{
    return read(s->fd, buf, len);
}

void gm10_serial_close(struct gm10_serial *s)
{
    if (s->fd < 0) return;
    (void)set_dtr(s->fd, 0);
    close(s->fd);
    s->fd = -1;
}
