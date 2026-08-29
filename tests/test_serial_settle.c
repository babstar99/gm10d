/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "serial.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    int p[2];
    struct gm10_serial serial;
    uint64_t discarded = 0;
    const unsigned char startup_bytes[] = {1, 2, 3, 4, 5, 6, 7};
    unsigned char byte;
    int flags;

    assert(pipe(p) == 0);
    flags = fcntl(p[0], F_GETFL, 0);
    assert(flags >= 0);
    assert(fcntl(p[0], F_SETFL, flags | O_NONBLOCK) == 0);
    assert(write(p[1], startup_bytes, sizeof(startup_bytes)) == (ssize_t)sizeof(startup_bytes));

    serial.fd = p[0];
    assert(gm10_serial_settle(&serial, 1, &discarded) == 0);
    assert(discarded == sizeof(startup_bytes));

    errno = 0;
    assert(read(p[0], &byte, 1) == -1);
    assert(errno == EAGAIN || errno == EWOULDBLOCK);

    close(p[1]);
    close(p[0]);
    puts("serial settle tests: ok");
    return 0;
}
