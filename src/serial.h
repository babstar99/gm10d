/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GM10D_SERIAL_H
#define GM10D_SERIAL_H

#include <stddef.h>
#include <sys/types.h>

struct gm10_serial {
    int fd;
};

void gm10_serial_init(struct gm10_serial *s);
int gm10_serial_open(struct gm10_serial *s, const char *device);
ssize_t gm10_serial_read(struct gm10_serial *s, void *buf, size_t len);
void gm10_serial_close(struct gm10_serial *s);

#endif
