/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GM10D_CONFIG_H
#define GM10D_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#define GM10D_STR_SMALL 128
#define GM10D_STR_MEDIUM 256

struct gm10_config {
    char device[PATH_MAX];
    char metrics_bind[GM10D_STR_SMALL];
    uint16_t metrics_port;

    bool mqtt_enable;
    char mqtt_host[GM10D_STR_MEDIUM];
    uint16_t mqtt_port;
    char mqtt_username[GM10D_STR_SMALL];
    char mqtt_password_file[PATH_MAX];
    char mqtt_topic_prefix[GM10D_STR_MEDIUM];
    char mqtt_device_id[GM10D_STR_SMALL];
    char mqtt_discovery_prefix[GM10D_STR_SMALL];
    unsigned mqtt_publish_interval;
    unsigned mqtt_keepalive;

    unsigned serial_retry_seconds;
    unsigned startup_settle_seconds;
};

void gm10_config_defaults(struct gm10_config *cfg);
int gm10_config_load(struct gm10_config *cfg, const char *path);
int gm10_config_validate(const struct gm10_config *cfg);

#endif
