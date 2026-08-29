/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "stats.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct gm10_stats s;
    gm10_stats_init(&s, 1000);

    gm10_stats_add(&s, 1000, 3);
    gm10_stats_add(&s, 1001, 2);
    assert(s.pulses_total == 5);
    assert(gm10_stats_window(&s, 1001, 60) == 5);
    assert(gm10_stats_window(&s, 1001, 10) == 5);

    gm10_stats_add(&s, 1011, 4);
    assert(s.pulses_total == 9);
    assert(gm10_stats_window(&s, 1011, 60) == 9);
    assert(gm10_stats_window(&s, 1011, 10) == 4);

    /* Old buckets must age out of the 60-second rolling window. */
    assert(gm10_stats_window(&s, 1060, 60) == 6); /* seconds 1001 and 1011 */
    assert(gm10_stats_window(&s, 1061, 60) == 4); /* second 1001 now aged out */

    /* Reusing a ring slot after 60 seconds must reset that bucket. */
    gm10_stats_add(&s, 1060, 7);
    assert(s.pulses_total == 16);
    assert(gm10_stats_window(&s, 1060, 60) == 13);

    puts("stats tests: ok");
    return 0;
}
