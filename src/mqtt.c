/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "mqtt.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_password(const char *path, char *buf, size_t bufsz)
{
    FILE *fp;
    size_t len;
    if (!path[0]) {
        buf[0] = '\0';
        return 0;
    }
    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "gm10d: cannot read MQTT password file %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (!fgets(buf, (int)bufsz, fp)) {
        fprintf(stderr, "gm10d: cannot read MQTT password from %s\n", path);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    len = strlen(buf);
    while (len && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    return 0;
}

static int pub(struct gm10_mqtt *m, const char *topic, const char *payload, bool retain)
{
    int rc = mosquitto_publish(m->mosq, NULL, topic, (int)strlen(payload), payload, 0, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "gm10d: MQTT publish %s failed: %s\n", topic, mosquitto_strerror(rc));
        return -1;
    }
    return 0;
}

static int publish_discovery_sensor(struct gm10_mqtt *m,
                                    const char *component,
                                    const char *name,
                                    const char *unit,
                                    const char *value_template,
                                    const char *state_class)
{
    char topic[512], payload[2048], unique_id[256];
    const struct gm10_config *c = m->cfg;
    snprintf(topic, sizeof(topic), "%s/sensor/%s_%s/config",
             c->mqtt_discovery_prefix, c->mqtt_device_id, component);
    snprintf(unique_id, sizeof(unique_id), "%s_%s", c->mqtt_device_id, component);

    int n = snprintf(payload, sizeof(payload),
        "{\"name\":\"%s\",\"unique_id\":\"%s\","
        "\"state_topic\":\"%s/state\",\"availability_topic\":\"%s/status\","
        "\"payload_available\":\"online\",\"payload_not_available\":\"offline\","
        "\"value_template\":\"{{ value_json.%s }}\","
        "%s%s%s"
        "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"Black Cat Systems GM-10\","
        "\"manufacturer\":\"Black Cat Systems\",\"model\":\"GM-10\"}}",
        name, unique_id,
        c->mqtt_topic_prefix, c->mqtt_topic_prefix,
        value_template,
        unit && *unit ? "\"unit_of_measurement\":\"" : "",
        unit && *unit ? unit : "",
        unit && *unit ? "\"," : "",
        c->mqtt_device_id);

    if (n < 0 || (size_t)n >= sizeof(payload)) return -1;

    /* Insert state_class before device if requested. */
    if (state_class && *state_class) {
        char with_class[2048];
        char *device = strstr(payload, "\"device\":");
        if (!device) return -1;
        size_t prefix = (size_t)(device - payload);
        int x = snprintf(with_class, sizeof(with_class), "%.*s\"state_class\":\"%s\",%s",
                         (int)prefix, payload, state_class, device);
        if (x < 0 || (size_t)x >= sizeof(with_class)) return -1;
        return pub(m, topic, with_class, true);
    }
    return pub(m, topic, payload, true);
}

static int publish_discovery(struct gm10_mqtt *m)
{
    int rc = 0;
    rc |= publish_discovery_sensor(m, "cpm", "Radiation CPM", "CPM", "cpm", "measurement");
    rc |= publish_discovery_sensor(m, "cps", "Radiation CPS", "CPS", "cps", "measurement");
    rc |= publish_discovery_sensor(m, "pulses_total", "Radiation Pulses", "events", "pulses_total", "total_increasing");
    return rc;
}

static void publish_availability(struct gm10_mqtt *m)
{
    char topic[512];
    if (!m->mosq || !atomic_load(&m->connected)) return;
    snprintf(topic, sizeof(topic), "%s/status", m->cfg->mqtt_topic_prefix);
    (void)pub(m, topic, atomic_load(&m->detector_connected) ? "online" : "offline", true);
}

static void on_connect(struct mosquitto *mosq, void *userdata, int rc)
{
    struct gm10_mqtt *m = userdata;
    (void)mosq;
    if (rc == 0) {
        atomic_store(&m->connected, true);
        fprintf(stderr, "gm10d: MQTT connected\n");
        publish_availability(m);
        (void)publish_discovery(m);
    } else {
        atomic_store(&m->connected, false);
        fprintf(stderr, "gm10d: MQTT connect rejected: %s\n", mosquitto_connack_string(rc));
    }
}

static void on_disconnect(struct mosquitto *mosq, void *userdata, int rc)
{
    struct gm10_mqtt *m = userdata;
    (void)mosq;
    atomic_store(&m->connected, false);
    if (rc != 0) fprintf(stderr, "gm10d: MQTT disconnected: %s\n", mosquitto_strerror(rc));
}

int gm10_mqtt_global_init(void)
{
    int rc = mosquitto_lib_init();
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "gm10d: mosquitto_lib_init: %s\n", mosquitto_strerror(rc));
        return -1;
    }
    return 0;
}

void gm10_mqtt_global_cleanup(void)
{
    mosquitto_lib_cleanup();
}

void gm10_mqtt_init(struct gm10_mqtt *m)
{
    memset(m, 0, sizeof(*m));
    atomic_init(&m->connected, false);
    atomic_init(&m->detector_connected, false);
}

int gm10_mqtt_start(struct gm10_mqtt *m, const struct gm10_config *cfg)
{
    int rc;
    char status_topic[512];

    if (!cfg->mqtt_enable) return 0;
    m->cfg = cfg;
    if (read_password(cfg->mqtt_password_file, m->password, sizeof(m->password))) return -1;

    m->mosq = mosquitto_new(cfg->mqtt_device_id, true, m);
    if (!m->mosq) {
        fprintf(stderr, "gm10d: mosquitto_new failed\n");
        return -1;
    }
    mosquitto_connect_callback_set(m->mosq, on_connect);
    mosquitto_disconnect_callback_set(m->mosq, on_disconnect);
    mosquitto_reconnect_delay_set(m->mosq, 2, 60, true);

    rc = mosquitto_int_option(m->mosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V311);
    if (rc != MOSQ_ERR_SUCCESS) goto fail;

    if (cfg->mqtt_username[0]) {
        rc = mosquitto_username_pw_set(m->mosq, cfg->mqtt_username,
                                       m->password[0] ? m->password : NULL);
        if (rc != MOSQ_ERR_SUCCESS) goto fail;
    }

    snprintf(status_topic, sizeof(status_topic), "%s/status", cfg->mqtt_topic_prefix);
    rc = mosquitto_will_set(m->mosq, status_topic, 7, "offline", 0, true);
    if (rc != MOSQ_ERR_SUCCESS) goto fail;

    rc = mosquitto_connect_async(m->mosq, cfg->mqtt_host, cfg->mqtt_port, (int)cfg->mqtt_keepalive);
    if (rc != MOSQ_ERR_SUCCESS) goto fail;
    rc = mosquitto_loop_start(m->mosq);
    if (rc != MOSQ_ERR_SUCCESS) goto fail;
    return 0;

fail:
    fprintf(stderr, "gm10d: MQTT setup failed: %s\n", mosquitto_strerror(rc));
    mosquitto_destroy(m->mosq);
    m->mosq = NULL;
    return -1;
}

void gm10_mqtt_stop(struct gm10_mqtt *m)
{
    if (!m->mosq) return;
    if (atomic_load(&m->connected)) {
        char topic[512];
        snprintf(topic, sizeof(topic), "%s/status", m->cfg->mqtt_topic_prefix);
        (void)pub(m, topic, "offline", true);
    }
    (void)mosquitto_disconnect(m->mosq);
    (void)mosquitto_loop_stop(m->mosq, true);
    mosquitto_destroy(m->mosq);
    m->mosq = NULL;
    atomic_store(&m->connected, false);
    memset(m->password, 0, sizeof(m->password));
}

bool gm10_mqtt_is_connected(const struct gm10_mqtt *m)
{
    return atomic_load(&m->connected);
}

void gm10_mqtt_set_detector_connected(struct gm10_mqtt *m, bool connected)
{
    atomic_store(&m->detector_connected, connected);
    publish_availability(m);
}

int gm10_mqtt_publish_state(struct gm10_mqtt *m,
                            const struct gm10_stats *stats,
                            uint64_t now_sec,
                            bool serial_connected)
{
    char topic[512], payload[1024];
    uint32_t cpm, counts10;
    double cps;
    int n;

    if (!m->mosq || !atomic_load(&m->connected)) return 0;
    cpm = gm10_stats_window(stats, now_sec, 60);
    counts10 = gm10_stats_window(stats, now_sec, 10);
    cps = cpm / 60.0;

    snprintf(topic, sizeof(topic), "%s/state", m->cfg->mqtt_topic_prefix);
    n = snprintf(payload, sizeof(payload),
        "{\"cpm\":%u,\"cps\":%.6f,\"counts_10s\":%u,\"cpm_10s\":%u,"
        "\"pulses_total\":%" PRIu64 ",\"serial_connected\":%s}",
        cpm, cps, counts10, counts10 * 6U, stats->pulses_total,
        serial_connected ? "true" : "false");
    if (n < 0 || (size_t)n >= sizeof(payload)) return -1;
    return pub(m, topic, payload, false);
}
