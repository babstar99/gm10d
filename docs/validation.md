# Real-hardware validation

`gm10d` was developed against a real Black Cat Systems GM-10 rather than a simulated protocol source.

## Native RS-232 reference run

Reference interface:

- Linux 16550A motherboard UART
- `/dev/ttyS0`
- 57600 8N1
- DTR detector power

Observed extended run:

- more than 51 hours continuous daemon uptime
- zero serial read errors
- zero serial reconnects
- MQTT continuously connected
- long-run background approximately 15.6 CPM for the test detector/location

A separate post-reboot run produced approximately 15.6 CPM over nearly seven hours, providing a repeatability check.

## StarTech USB-to-RS232 run

Validated adapter:

- StarTech ICUSB2321F
- FTDI FT232R
- USB `0403:6001`
- Linux `ftdi_sio`
- `/dev/serial/by-id/...`

Initial extended observation:

- 56,916 seconds (~15.8 hours) uptime
- 14,371 cumulative events
- calculated long-run mean approximately 15.15 CPM
- zero serial read errors
- zero serial reconnects
- MQTT connected

The USB result is consistent with the native-UART background range, supporting use of the FTDI adapter as a long-term replacement for machines without physical RS-232 ports.

## Integrations validated

The same deployment has been exercised end-to-end with:

- systemd service supervision
- Mosquitto authentication and ACLs
- Home Assistant MQTT discovery and live entities
- a bridged Mosquitto topology
- remote vmagent scraping
- VictoriaMetrics stream aggregation
- Grafana CPM graphing

## What this validation does not mean

The observed CPM values are **not a calibration standard**. They establish software/interface consistency only. Absolute detector response depends on the tube, radiation energy spectrum, geometry, shielding, and calibration.
