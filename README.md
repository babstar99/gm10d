# gm10d

`gm10d` is a small Linux daemon for the **Black Cat Systems GM-10** serial radiation detector.
It turns the detector's raw serial pulse stream into useful long-running telemetry without a Python/pip stack.

**Current version:** 0.1.3  
**License:** GPL-2.0-or-later  
**Runtime:** C + `libmosquitto` + the normal Linux/POSIX runtime

## What it does

- reads a GM-10 from a native RS-232 port or a USB-to-RS232 adapter
- reproduces the original `gm4lin` hardware setup: **57600 baud, 8N1, DTR asserted to power the detector**
- counts the actual number of received serial bytes, where each byte represents one detected event
- exports a Prometheus-compatible `/metrics` endpoint for vmagent / VictoriaMetrics / Prometheus
- publishes MQTT state, availability, and Home Assistant discovery via `libmosquitto`
- runs in the foreground under systemd with automatic restart and serial reconnect
- deliberately keeps the dependency set small and native

## Architecture

```text
Black Cat Systems GM-10
        |
        | RS-232, 57600 8N1
        | DTR provides detector power
        v
      gm10d
        |
        +---- MQTT ----> Mosquitto ----> Home Assistant
        |
        +---- /metrics -> vmagent -----> VictoriaMetrics ----> Grafana
```

The raw cumulative event counter is kept as the authoritative observation. CPM/CPS are derived from the received event stream.

## Hardware support

### Native serial

```ini
device=/dev/ttyS0
```

Validated on a Linux 16550A motherboard UART.

### USB-to-RS232

Use a **real USB-to-RS232 adapter** with proper RS-232 voltage levels and DTR. Do **not** use a USB-to-3.3 V/5 V TTL UART cable.

For permanent installations, use Linux's stable `/dev/serial/by-id/` path instead of `/dev/ttyUSB0`:

```ini
device=/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_<SERIAL>-if00-port0
```

Validated hardware:

- **StarTech ICUSB2321F** — FTDI FT232R (`0403:6001`), Linux `ftdi_sio`, tested with a real GM-10
- native 16550A RS-232 UART

See [docs/hardware.md](docs/hardware.md) for details and validation results.

## gm4lin lineage

`gm10d` uses `gm4lin 1.2.15` by Mathias Bavay as the authoritative historical reference for the GM-10/GM-45 Linux serial setup. The original source establishes:

- 57600 baud
- 8 data bits, no parity, one stop bit
- receiver/local mode enabled
- DTR asserted to power the probe
- DTR lowered on shutdown

`gm10d` intentionally improves normal activity counting. Historical `gm4lin` increments once when the serial fd becomes readable and then drains up to 8 bytes. `gm10d` instead increments by the **actual number of bytes returned by `read()`**, preserving one received byte as one particle event.

More detail: [docs/gm4lin-hardware-notes.md](docs/gm4lin-hardware-notes.md).

## Dependencies

Debian 13 runtime:

```bash
sudo apt install libmosquitto1
```

Build requirements:

```bash
sudo apt install build-essential libmosquitto-dev
```

No Python, pip, virtualenv, database, container runtime, JSON library, HTTP framework, or language-specific package ecosystem is required.

## Build and test

```bash
make
make check
./gm10d --version
```

For stricter local compilation:

```bash
make clean
make WERROR=1
make check WERROR=1
```

## Install

Create a dedicated service account:

```bash
sudo useradd --system --user-group --no-create-home --shell /usr/sbin/nologin gm10
sudo usermod -aG dialout gm10
```

Install the binary, example config, and service unit:

```bash
sudo make install
```

Create the live config:

```bash
sudo cp /etc/gm10d.conf.example /etc/gm10d.conf
sudo editor /etc/gm10d.conf
```

Create the MQTT password file without putting the password in the config:

```bash
sudo sh -c 'umask 077; cat > /etc/gm10d.mqtt-password'
# Paste password, press Enter, then Ctrl-D.
sudo chown root:gm10 /etc/gm10d.mqtt-password
sudo chmod 640 /etc/gm10d.mqtt-password
```

Start the daemon:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now gm10d
systemctl status gm10d
```

Logs:

```bash
sudo journalctl -fu gm10d
```

## Example configuration

```ini
device=/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_<SERIAL>-if00-port0

metrics_bind=0.0.0.0
metrics_port=9798

mqtt_enable=true
mqtt_host=192.168.1.10
mqtt_port=1883
mqtt_username=gm10
mqtt_password_file=/etc/gm10d.mqtt-password
mqtt_topic_prefix=gm10/gm10-01
mqtt_device_id=gm10-01
mqtt_discovery_prefix=homeassistant
mqtt_publish_interval=10
mqtt_keepalive=60

startup_settle_seconds=2
serial_retry_seconds=5
```

The detector is powered by asserting DTR. After power-up, `gm10d` waits for `startup_settle_seconds`, flushes queued RX input, and then begins counting. This prevents the GM-10 power-up transient from contaminating CPM and the cumulative event counter.

## Metrics

Default endpoint:

```text
http://HOST:9798/metrics
```

Exported metrics include:

| Metric | Type | Meaning |
|---|---|---|
| `gm10_pulses_total` | counter | Particle events received since process start |
| `gm10_cpm` | gauge | Actual events in the trailing 60 seconds |
| `gm10_cps` | gauge | `gm10_cpm / 60` |
| `gm10_counts_10s` | gauge | Events in the trailing 10 seconds |
| `gm10_cpm_10s` | gauge | 10-second count × 6, compatible with gm4lin-style reporting |
| `gm10_serial_connected` | gauge | Serial detector connection state |
| `gm10_mqtt_connected` | gauge | MQTT connection state |
| `gm10_serial_read_errors_total` | counter | Serial read/HUP errors |
| `gm10_serial_reconnects_total` | counter | Successful serial reconnects after startup |
| `gm10_uptime_seconds` | gauge | Process uptime |

VictoriaMetrics examples are in [docs/victoriametrics.md](docs/victoriametrics.md).

## MQTT / Home Assistant

Default topics:

```text
gm10/gm10-01/state
gm10/gm10-01/status

homeassistant/sensor/gm10-01_cpm/config
homeassistant/sensor/gm10-01_cps/config
homeassistant/sensor/gm10-01_pulses_total/config
```

Home Assistant discovery creates one **Black Cat Systems GM-10** device with:

- Radiation CPM
- Radiation CPS
- Radiation Pulses

Discovery and availability messages are retained. Measurement state is deliberately not retained.

See [docs/mqtt-home-assistant.md](docs/mqtt-home-assistant.md) for ACL and broker-bridge examples.

## Dose and calibration

`gm10d` intentionally reports **counts**, not an unqualified dose estimate. The historical `gm4lin` source contains GM-10 Co-60/Cs-137 sensitivity values, but conversion from CPM to dose depends on detector response, radiation energy, geometry, and calibration.

For environmental monitoring, the most useful signal is often deviation from the detector's established local baseline. Preserve raw CPM and `gm10_pulses_total` even if a calibrated dose conversion is added later.

## Security

The `/metrics` endpoint has no authentication or TLS. Bind it to `127.0.0.1` unless remote scraping is required; for LAN scraping, restrict access with normal host/network controls. MQTT credentials should be scoped with broker ACLs and stored in a mode-0640 password file.

See [SECURITY.md](SECURITY.md).

## Project status

Version 0.1.3 has been validated end-to-end on real hardware with systemd, Mosquitto, Home Assistant MQTT discovery, vmagent, VictoriaMetrics, and Grafana. See [docs/validation.md](docs/validation.md).

## License

`gm10d` is licensed under **GPL-2.0-or-later**. See [LICENSE](LICENSE), [AUTHORS](AUTHORS), and [NOTICE](NOTICE).
