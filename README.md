# gm10d

`gm10d` is a small Linux daemon for the **Black Cat Systems GM-10** serial radiation detector.
It is intended for long-running environmental monitoring and exports raw count data to:

- Prometheus-compatible `/metrics` for **VictoriaMetrics/vmagent**
- **MQTT** with Home Assistant discovery

It is written in C, runs in the foreground under systemd, and deliberately avoids interpreter/package-manager stacks.

## Design goals

- Linux/POSIX C
- one intentional direct runtime library dependency: `libmosquitto` (plus the normal C runtime)
- no Python, pip, virtualenv, container, database, or web framework
- native motherboard RS-232 and USB-to-RS232 through the same Linux TTY interface
- preserve raw particle counts as the authoritative measurement
- discard detector power-up serial transients before acquisition begins
- robust enough for unattended systemd operation

## gm4lin lineage

The hardware behaviour is based on `gm4lin 1.2.15` by Mathias Bavay (2003), released under GPL-2.0-or-later.
The original source establishes:

- 57600 baud
- 8 data bits, no parity, one stop bit (8N1)
- receiver/local mode enabled
- DTR asserted **to provide power to the probe**
- DTR lowered on shutdown

`gm10d` intentionally improves normal activity counting. Historical `gm4lin` increments the count once when the serial fd becomes readable and then drains up to 8 bytes. Black Cat's interface represents each detected particle as serial data, so `gm10d` increments the event count by the **actual number of bytes returned by `read()`**.

## Hardware

### Native serial

```ini
device=/dev/ttyS0
```

### USB-to-RS232

Use a **real USB-to-RS232 adapter** with proper RS-232 signalling and DTR. A USB-to-3.3V/5V TTL UART cable is not equivalent and is not suitable.

Linux normally exposes a USB serial adapter as `/dev/ttyUSB0`. For a permanent installation prefer the stable udev symlink:

```ini
device=/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A123456-if00-port0
```

The acquisition code is identical for native and USB serial ports because both use the Linux TTY API.

### Detector startup settling

`gm10d` asserts DTR to power the detector, waits for a short configurable settle period, and discards serial input received during that interval before acquisition begins. The default is:

```ini
startup_settle_seconds=2
```

This was added after real GM-10 hardware testing showed a repeatable power-up burst that temporarily inflated CPM and `pulses_total`. The discarded bytes are exposed separately as a diagnostic and are never added to the radiation count.

## Dependencies

Direct runtime dependency on Debian 13:

```bash
sudo apt install libmosquitto1
```

Debian manages any transitive libraries required by `libmosquitto`; there is no pip/virtualenv or language package stack.

Build requirements:

```bash
sudo apt install build-essential libmosquitto-dev
```

## Build

```bash
make
./gm10d --version
```

## Configuration

Copy the example:

```bash
sudo cp gm10d.conf.example /etc/gm10d.conf
```

For MQTT credentials, keep the password readable only by root and the dedicated `gm10` service group:

```bash
sudo sh -c 'printf "%s\\n" "YOUR_PASSWORD" > /etc/gm10d.mqtt-password'
sudo chown root:gm10 /etc/gm10d.mqtt-password
sudo chmod 640 /etc/gm10d.mqtt-password
```

## Tests

The statistics/ring-buffer logic and startup-settle drain have dependency-free unit tests:

```bash
make check
```

## systemd

Create the service account:

```bash
sudo useradd --system --user-group --no-create-home --shell /usr/sbin/nologin gm10
sudo usermod -aG dialout gm10
```

Install and start:

```bash
sudo make install
sudo cp /etc/gm10d.conf.example /etc/gm10d.conf
sudo systemctl daemon-reload
sudo systemctl enable --now gm10d
```

Inspect:

```bash
systemctl status gm10d
journalctl -fu gm10d
```

## Metrics

Default endpoint in the example config:

```text
http://HOST:9798/metrics
```

Metrics include:

- `gm10_pulses_total` — cumulative raw serial particle events since process start
- `gm10_cpm` — actual events in the preceding rolling 60 seconds
- `gm10_cps` — `gm10_cpm / 60`
- `gm10_counts_10s` — events in the preceding 10 seconds
- `gm10_cpm_10s` — 10-second count multiplied by six, compatible with gm4lin's normal reporting method
- `gm10_serial_connected`
- `gm10_mqtt_connected`
- serial error/reconnect counters
- `gm10_startup_discarded_bytes_total` — bytes discarded during detector startup settle periods

Example vmagent scrape config:

```yaml
- job_name: gm10
  static_configs:
    - targets:
        - 'GM10_HOST:9798'
```

For longer smoothing windows, prefer deriving CPM from the cumulative counter in VictoriaMetrics rather than discarding the raw measurement.

## MQTT / Home Assistant

State is published to:

```text
gm10/gm10-01/state
```

Availability is retained at:

```text
gm10/gm10-01/status
```

The daemon publishes Home Assistant MQTT discovery records for CPM, CPS, and total pulses. The retained availability topic follows the GM-10 serial connection, so the radiation entities become unavailable if the detector is unplugged or the daemon dies. Sensor state messages themselves are not retained. The state JSON also includes `startup_discarded_bytes_total` for diagnostics.

## Dose conversion

Version 0.1 deliberately does **not** publish dose or exposure estimates. `gm4lin` contains historical GM-10 sensitivity values for Co-60 and Cs-137, but CPM is the direct measurement and is retained without silently applying an isotope-dependent calibration.

## License

GPL-2.0-or-later. See `LICENSE` and `AUTHORS`.
