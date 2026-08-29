/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "serial.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

int main(void)
{
    int master_fd, slave_fd;
    char *slave_name;
    struct gm10_serial serial;
    const unsigned char startup_bytes[] = {1, 2, 3, 4, 5, 6, 7};
    unsigned char byte;
    struct termios tty;

    master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    assert(master_fd >= 0);
    assert(grantpt(master_fd) == 0);
    assert(unlockpt(master_fd) == 0);
    slave_name = ptsname(master_fd);
    assert(slave_name != NULL);

    slave_fd = open(slave_name, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    assert(slave_fd >= 0);

    assert(tcgetattr(slave_fd, &tty) == 0);
    cfmakeraw(&tty);
    assert(tcsetattr(slave_fd, TCSANOW, &tty) == 0);

    assert(write(master_fd, startup_bytes, sizeof(startup_bytes)) ==
           (ssize_t)sizeof(startup_bytes));

    serial.fd = slave_fd;
    assert(gm10_serial_settle(&serial, 0) == 0);

    errno = 0;
    assert(read(slave_fd, &byte, 1) == -1);
    assert(errno == EAGAIN || errno == EWOULDBLOCK);

    /* New input arriving after the flush must remain readable. */
    byte = 42;
    assert(write(master_fd, &byte, 1) == 1);
    byte = 0;
    assert(read(slave_fd, &byte, 1) == 1);
    assert(byte == 42);

    close(slave_fd);
    close(master_fd);
    puts("serial settle tests: ok");
    return 0;
}
