/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "stats.h"

#include <string.h>

void gm10_stats_init(struct gm10_stats *s, uint64_t now_sec)
{
    memset(s, 0, sizeof(*s));
    s->started_monotonic_sec = now_sec;
}

void gm10_stats_record_serial_read(struct gm10_stats *s, uint32_t bytes)
{
    if (bytes == 0) return;

    s->serial_read_calls_total++;
    s->serial_bytes_total += bytes;

    if (bytes > 1) {
        s->serial_multibyte_reads_total++;
        s->serial_multibyte_bytes_total += bytes;
    }

    if (bytes == 1) {
        s->serial_reads_1byte_total++;
    } else if (bytes <= 4) {
        s->serial_reads_2_4bytes_total++;
    } else if (bytes <= 16) {
        s->serial_reads_5_16bytes_total++;
    } else {
        s->serial_reads_gt16bytes_total++;
    }

    if (bytes > s->serial_max_read_size_bytes)
        s->serial_max_read_size_bytes = bytes;
}

void gm10_stats_add(struct gm10_stats *s, uint64_t now_sec, uint32_t count)
{
    unsigned i = (unsigned)(now_sec % GM10D_BUCKETS);
    if (s->bucket_second[i] != now_sec) {
        s->bucket_second[i] = now_sec;
        s->bucket_count[i] = 0;
    }
    s->bucket_count[i] += count;
    if (s->bucket_count[i] > s->max_events_1s)
        s->max_events_1s = s->bucket_count[i];
    s->pulses_total += count;
}

uint32_t gm10_stats_window(const struct gm10_stats *s, uint64_t now_sec, unsigned seconds)
{
    uint64_t total = 0;
    if (seconds > GM10D_BUCKETS) seconds = GM10D_BUCKETS;
    for (unsigned i = 0; i < GM10D_BUCKETS; i++) {
        uint64_t t = s->bucket_second[i];
        if (t <= now_sec && now_sec - t < seconds) total += s->bucket_count[i];
    }
    return total > UINT32_MAX ? UINT32_MAX : (uint32_t)total;
}
