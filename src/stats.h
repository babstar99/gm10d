/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GM10D_STATS_H
#define GM10D_STATS_H

#include <stdint.h>

#define GM10D_BUCKETS 60

struct gm10_stats {
    uint64_t pulses_total;
    uint64_t read_errors_total;
    uint64_t reconnects_total;
    uint64_t serial_read_calls_total;
    uint64_t serial_bytes_total;
    uint64_t serial_multibyte_reads_total;
    uint64_t serial_multibyte_bytes_total;
    uint64_t serial_reads_1byte_total;
    uint64_t serial_reads_2_4bytes_total;
    uint64_t serial_reads_5_16bytes_total;
    uint64_t serial_reads_gt16bytes_total;
    uint32_t serial_max_read_size_bytes;
    uint32_t max_events_1s;
    uint64_t started_monotonic_sec;
    uint64_t bucket_second[GM10D_BUCKETS];
    uint32_t bucket_count[GM10D_BUCKETS];
};

void gm10_stats_init(struct gm10_stats *s, uint64_t now_sec);
void gm10_stats_record_serial_read(struct gm10_stats *s, uint32_t bytes);
void gm10_stats_add(struct gm10_stats *s, uint64_t now_sec, uint32_t count);
uint32_t gm10_stats_window(const struct gm10_stats *s, uint64_t now_sec, unsigned seconds);

#endif
