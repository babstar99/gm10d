/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char *trim(char *s)
{
    char *end;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static int copy_string(char *dst, size_t dstsz, const char *src, const char *key)
{
    int n = snprintf(dst, dstsz, "%s", src);
    if (n < 0 || (size_t)n >= dstsz) {
        fprintf(stderr, "gm10d: config value for %s is too long\n", key);
        return -1;
    }
    return 0;
}

static int parse_u16(const char *s, uint16_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno || !end || *end != '\0' || v == 0 || v > 65535) return -1;
    *out = (uint16_t)v;
    return 0;
}

static int parse_uint(const char *s, unsigned *out, unsigned min, unsigned max)
{
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno || !end || *end != '\0' || v < min || v > max) return -1;
    *out = (unsigned)v;
    return 0;
}

static int parse_bool(const char *s, bool *out)
{
    if (!strcasecmp(s, "true") || !strcasecmp(s, "yes") || !strcmp(s, "1")) {
        *out = true;
        return 0;
    }
    if (!strcasecmp(s, "false") || !strcasecmp(s, "no") || !strcmp(s, "0")) {
        *out = false;
        return 0;
    }
    return -1;
}

void gm10_config_defaults(struct gm10_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->device, sizeof(cfg->device), "/dev/ttyS0");
    snprintf(cfg->metrics_bind, sizeof(cfg->metrics_bind), "127.0.0.1");
    cfg->metrics_port = 9798;

    cfg->mqtt_enable = false;
    cfg->mqtt_port = 1883;
    snprintf(cfg->mqtt_topic_prefix, sizeof(cfg->mqtt_topic_prefix), "gm10/gm10-01");
    snprintf(cfg->mqtt_device_id, sizeof(cfg->mqtt_device_id), "gm10-01");
    snprintf(cfg->mqtt_discovery_prefix, sizeof(cfg->mqtt_discovery_prefix), "homeassistant");
    cfg->mqtt_publish_interval = 10;
    cfg->mqtt_keepalive = 60;

    cfg->serial_retry_seconds = 5;
    cfg->startup_settle_seconds = 2;
}

int gm10_config_load(struct gm10_config *cfg, const char *path)
{
    FILE *fp = fopen(path, "r");
    char line[1024];
    char *key = NULL;
    unsigned lineno = 0;

    if (!fp) {
        fprintf(stderr, "gm10d: cannot open config %s: %s\n", path, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *p, *eq, *value;
        lineno++;
        p = trim(line);
        if (*p == '\0' || *p == '#' || *p == ';') continue;
        eq = strchr(p, '=');
        if (!eq) {
            fprintf(stderr, "gm10d: %s:%u: expected key=value\n", path, lineno);
            fclose(fp);
            return -1;
        }
        *eq = '\0';
        key = trim(p);
        value = trim(eq + 1);

#define SETSTR(name, field) if (!strcmp(key, name)) { if (copy_string(cfg->field, sizeof(cfg->field), value, key)) goto bad; continue; }
        SETSTR("device", device)
        SETSTR("metrics_bind", metrics_bind)
        SETSTR("mqtt_host", mqtt_host)
        SETSTR("mqtt_username", mqtt_username)
        SETSTR("mqtt_password_file", mqtt_password_file)
        SETSTR("mqtt_topic_prefix", mqtt_topic_prefix)
        SETSTR("mqtt_device_id", mqtt_device_id)
        SETSTR("mqtt_discovery_prefix", mqtt_discovery_prefix)
#undef SETSTR

        if (!strcmp(key, "metrics_port")) {
            if (parse_u16(value, &cfg->metrics_port)) goto bad;
        } else if (!strcmp(key, "mqtt_enable")) {
            if (parse_bool(value, &cfg->mqtt_enable)) goto bad;
        } else if (!strcmp(key, "mqtt_port")) {
            if (parse_u16(value, &cfg->mqtt_port)) goto bad;
        } else if (!strcmp(key, "mqtt_publish_interval")) {
            if (parse_uint(value, &cfg->mqtt_publish_interval, 1, 3600)) goto bad;
        } else if (!strcmp(key, "mqtt_keepalive")) {
            if (parse_uint(value, &cfg->mqtt_keepalive, 5, 3600)) goto bad;
        } else if (!strcmp(key, "serial_retry_seconds")) {
            if (parse_uint(value, &cfg->serial_retry_seconds, 1, 3600)) goto bad;
        } else if (!strcmp(key, "startup_settle_seconds")) {
            if (parse_uint(value, &cfg->startup_settle_seconds, 0, 60)) goto bad;
        } else {
            fprintf(stderr, "gm10d: %s:%u: unknown key '%s'\n", path, lineno, key);
            fclose(fp);
            return -1;
        }
    }

    if (ferror(fp)) {
        fprintf(stderr, "gm10d: error reading %s\n", path);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;

bad:
    fprintf(stderr, "gm10d: %s:%u: invalid value for '%s'\n", path, lineno, key);
    fclose(fp);
    return -1;
}

static bool valid_id(const char *s)
{
    if (!*s) return false;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (!(isalnum(c) || c == '_' || c == '-')) return false;
    }
    return true;
}

static bool valid_topic_prefix(const char *s)
{
    if (!*s) return false;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c == 0x7f || c == '#' || c == '+' || c == '\"' || c == '\\') return false;
    }
    return true;
}

int gm10_config_validate(const struct gm10_config *cfg)
{
    if (!cfg->device[0]) {
        fprintf(stderr, "gm10d: device must not be empty\n");
        return -1;
    }
    if (cfg->mqtt_enable) {
        if (!cfg->mqtt_host[0]) {
            fprintf(stderr, "gm10d: mqtt_host is required when mqtt_enable=true\n");
            return -1;
        }
        if (!valid_id(cfg->mqtt_device_id)) {
            fprintf(stderr, "gm10d: mqtt_device_id may contain only letters, digits, '_' and '-'\n");
            return -1;
        }
        if (!valid_topic_prefix(cfg->mqtt_topic_prefix) || !valid_topic_prefix(cfg->mqtt_discovery_prefix)) {
            fprintf(stderr, "gm10d: MQTT topic prefixes must be non-empty and contain no wildcards, quotes, backslashes, or control characters\n");
            return -1;
        }
    }
    return 0;
}
