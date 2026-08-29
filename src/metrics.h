/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GM10D_METRICS_H
#define GM10D_METRICS_H

#include <stdbool.h>
#include <stdint.h>

#include "stats.h"

struct gm10_metrics_server {
    int fd;
};

void gm10_metrics_init(struct gm10_metrics_server *m);
int gm10_metrics_open(struct gm10_metrics_server *m, const char *bind_addr, uint16_t port);
void gm10_metrics_close(struct gm10_metrics_server *m);
void gm10_metrics_serve_one(struct gm10_metrics_server *m,
                            const struct gm10_stats *stats,
                            uint64_t now_sec,
                            bool serial_connected,
                            bool mqtt_enabled,
                            bool mqtt_connected);

#endif
