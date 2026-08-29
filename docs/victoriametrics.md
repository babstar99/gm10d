# VictoriaMetrics / vmagent

`gm10d` exposes Prometheus text format on `/metrics`, so vmagent can scrape it directly.

## Basic scrape job

```yaml
scrape_configs:
  - job_name: gm10
    scheme: http
    metrics_path: /metrics
    scrape_interval: 15s
    scrape_timeout: 5s

    static_configs:
      - targets:
          - 'GM10_HOST:9798'
        labels:
          source: gm10
```

A 15-second scrape provides responsive monitoring while remaining trivial load for gm10d and vmagent.

## One-minute stream aggregation

If the storage policy is to retain one sample per minute:

```yaml
- name: gm10_1minute
  match: '{__name__=~"gm10_.*"}'
  interval: 1m
  outputs: [last]
  keep_metric_names: true
```

`last` is suitable for the exported set because it preserves cumulative counters such as `gm10_pulses_total` and health state values without averaging a monotonic counter.

## Useful queries

Current detector rate:

```promql
gm10_cpm
```

Five-minute rate derived directly from the cumulative event counter:

```promql
rate(gm10_pulses_total[5m]) * 60
```

One-hour smoothed CPM from the daemon's rolling CPM:

```promql
avg_over_time(gm10_cpm[1h])
```

Serial health:

```promql
gm10_serial_connected
```

Serial errors over a period:

```promql
increase(gm10_serial_read_errors_total[24h])
```

Reconnects over a period:

```promql
increase(gm10_serial_reconnects_total[24h])
```

## Baseline monitoring

CPM is detector-specific. For environmental monitoring, establish a long-running baseline for the same detector in the same location, then alert on **sustained deviation from that baseline** rather than comparing raw CPM directly with unrelated detector models.
