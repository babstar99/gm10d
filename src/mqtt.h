/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GM10D_MQTT_H
#define GM10D_MQTT_H

#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <mosquitto.h>

#include "config.h"
#include "stats.h"

struct gm10_mqtt {
    struct mosquitto *mosq;
    atomic_bool connected;
    atomic_bool detector_connected;
    const struct gm10_config *cfg;
    char password[GM10D_STR_MEDIUM];
};

int gm10_mqtt_global_init(void);
void gm10_mqtt_global_cleanup(void);
void gm10_mqtt_init(struct gm10_mqtt *m);
int gm10_mqtt_start(struct gm10_mqtt *m, const struct gm10_config *cfg);
void gm10_mqtt_stop(struct gm10_mqtt *m);
bool gm10_mqtt_is_connected(const struct gm10_mqtt *m);
void gm10_mqtt_set_detector_connected(struct gm10_mqtt *m, bool connected);
int gm10_mqtt_publish_state(struct gm10_mqtt *m,
                            const struct gm10_stats *stats,
                            uint64_t now_sec,
                            bool serial_connected);

#endif
