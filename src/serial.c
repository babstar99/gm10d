/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hardware behaviour follows gm4lin 1.2.15 by Mathias Bavay (2003):
 * 57600 baud, 8N1, receiver enabled, and DTR asserted to power the probe.
 */
#include "serial.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
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
