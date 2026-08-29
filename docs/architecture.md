# Architecture

`gm10d` is intentionally small and event-driven.

```text
                  +-------------------------+
                  |         gm10d           |
                  |                         |
GM-10 TTY --------+--> serial acquisition  |
                  |       |                 |
                  |       v                 |
                  |   rolling stats         |
                  |    /        \\          |
                  |   v          v          |
                  | MQTT       HTTP         |
                  +---+----------+----------+
                      |          |
                      v          v
                  Mosquitto   vmagent
                      |          |
                      v          v
                Home Assistant VictoriaMetrics
                                 |
                                 v
                               Grafana
```

## Serial acquisition

The main process uses `poll()` rather than a busy loop. When the TTY is readable, `gm10d` drains available bytes and increments the event count by the number of bytes actually returned by `read()`.

This is intentionally different from historical `gm4lin` normal activity mode, which increments once per readable event and then drains multiple bytes.

## Statistics

A small one-second ring buffer retains recent event counts. It is used to derive:

- trailing 60-second CPM
- CPS (`CPM / 60`)
- trailing 10-second event count
- 10-second extrapolated CPM for gm4lin compatibility

`gm10_pulses_total` is a process-lifetime monotonic counter and is the best source for deriving arbitrary longer-window rates in a time-series database.

## MQTT

MQTT is delegated to `libmosquitto`. It owns a small network thread while the main process remains responsible for serial acquisition and the metrics listener.

The daemon uses MQTT 3.1.1, Last Will availability, automatic reconnect, retained Home Assistant discovery, and non-retained measurement state.

## HTTP metrics

The HTTP server is deliberately minimal and only serves `/metrics`. It uses normal POSIX sockets rather than an HTTP framework.

Version 0.1.3 fixed a remote-scrape timing race by waiting briefly for complete request headers after `accept()` before sending the response. This was discovered with real vmagent scraping over the LAN.

## Failure boundaries

Serial, MQTT, and metrics are kept loosely coupled:

- MQTT failure does not stop serial acquisition.
- A missing/unplugged detector leaves the process alive and the metrics endpoint available.
- Serial reconnect is retried on a configurable interval.
- The MQTT availability topic follows detector connectivity.
- systemd supervises the process and restarts it on failure.
