# Changelog

## 0.1.4

- Add diagnostic instrumentation for successful serial reads without changing GM-10 event-counting behaviour.
- Export total serial read calls and bytes, multi-byte read calls and bytes, read-size buckets, and the largest successful read seen since process start.
- Export the largest number of particle events observed in any one-second bucket since process start.
- Keep every received byte counted exactly as before; this release does not filter, reject, merge, or reinterpret detector events.
- Add regression coverage for the serial-read diagnostics and Prometheus output.
- Document real-hardware known-source validation using an independently identified Tc-99m source, including v0.1.4 serial-read diagnostics.

## 0.1.3

- Fix an HTTP scrape race where the metrics server used a non-blocking `recv()` immediately after `accept()`. A remote scraper such as vmagent could complete the TCP handshake before its HTTP request bytes reached the receive queue, causing gm10d to close the connection early and the scraper to report `connection reset by peer`.
- Wait briefly for and consume the complete HTTP request headers before responding, preventing TCP resets caused by unread request data on close.
- Add a regression test that deliberately delays the HTTP request after connecting, reproducing the vmagent timing pattern.
- Return `404 Not Found` for paths other than `/metrics`.

## 0.1.2

- Simplify detector startup settling after real-hardware validation.
- After DTR assertion, wait `startup_settle_seconds` and then discard queued RX input with `tcflush(TCIFLUSH)` before acquisition begins.
- Remove `gm10_startup_discarded_bytes_total`; startup electrical activity is intentionally not interpreted as meaningful serial bytes.
- Preserve the validated 57600 8N1, DTR power, byte-accurate counting, MQTT/Home Assistant, and Prometheus/VictoriaMetrics behaviour.

## 0.1.1

- Add configurable `startup_settle_seconds` (default 2 seconds).
- Discard serial bytes received while the GM-10 high-voltage/power circuitry settles after DTR assertion.
- Prevent startup transients from contaminating rolling CPM and `gm10_pulses_total`.
- Export `gm10_startup_discarded_bytes_total` in Prometheus metrics and MQTT state for diagnostics.
- Apply the same settle procedure after serial reconnects.
- Run the systemd service with primary group `gm10` plus supplementary `dialout`, allowing a `root:gm10` mode-0640 MQTT password file.

## 0.1.0

- Initial gm10d implementation.
- Linux TTY support for native serial and USB-to-RS232 devices.
- gm4lin-compatible 57600 8N1 serial setup and DTR detector power control.
- Byte-accurate particle counting.
- Rolling 60-second CPM and 10-second gm4lin-compatible CPM.
- Prometheus/VictoriaMetrics HTTP metrics endpoint.
- MQTT 3.1.1 via libmosquitto with retained availability and Home Assistant discovery.
- systemd service and example configuration.
