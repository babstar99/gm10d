# Changelog

## 0.1.0

- Initial gm10d implementation.
- Linux TTY support for native serial and USB-to-RS232 devices.
- gm4lin-compatible 57600 8N1 serial setup and DTR detector power control.
- Byte-accurate particle counting.
- Rolling 60-second CPM and 10-second gm4lin-compatible CPM.
- Prometheus/VictoriaMetrics HTTP metrics endpoint.
- MQTT 3.1.1 via libmosquitto with retained availability and Home Assistant discovery.
- systemd service and example configuration.
