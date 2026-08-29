# Changelog

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
