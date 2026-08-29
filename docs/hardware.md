# Hardware

## GM-10 serial interface

The hardware setup used by `gm10d` follows the original `gm4lin 1.2.15` implementation:

- 57600 baud
- 8 data bits
- no parity
- 1 stop bit
- no hardware flow control
- receiver enabled
- DTR asserted while the detector is in service

The GM-10 uses the RS-232 interface as both its communications path and part of its power arrangement. `gm10d` therefore treats DTR as part of the detector lifecycle: it asserts DTR after opening/configuring the port and clears DTR before closing it.

Each received byte is counted as one detector event. The byte value itself is not interpreted.

## Native UART

A conventional PC RS-232 port is the simplest reference configuration:

```ini
device=/dev/ttyS0
```

The reference deployment was tested on a Linux 16550A UART and ran for more than 51 continuous hours with:

- `gm10_serial_read_errors_total = 0`
- `gm10_serial_reconnects_total = 0`
- stable long-run background around 15–16 CPM for that detector/location

The absolute CPM figure is **not** a universal background reference; it is included only to demonstrate consistency between interfaces.

## USB-to-RS232

Linux exposes supported USB serial adapters through the normal TTY API, so `gm10d` does not need a separate USB acquisition backend.

Typical paths are:

```text
/dev/ttyUSB0
/dev/serial/by-id/...
```

For a permanent installation, prefer `/dev/serial/by-id/` because `/dev/ttyUSB0` may change when other adapters are added.

### Validated adapter: StarTech ICUSB2321F

The following adapter has been tested on real GM-10 hardware:

- Model: **StarTech ICUSB2321F**
- USB UART: **FTDI FT232R**
- USB VID:PID: `0403:6001`
- Linux driver: `ftdi_sio`
- Persistent device path format:

```text
/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_<SERIAL>-if00-port0
```

Example discovery commands:

```bash
lsusb
ls -l /dev/ttyUSB*
ls -l /dev/serial/by-id/
udevadm info --query=property --name=/dev/ttyUSB0 | \
  grep -E 'ID_VENDOR=|ID_MODEL=|ID_SERIAL='
```

In validation testing the FTDI path produced the same expected background count range as the native UART, with zero serial read errors and zero reconnects over the initial extended run.

## What not to use

Do not confuse **USB-to-RS232** with a TTL UART adapter. A TTL cable typically exposes 3.3 V or 5 V logic-level TX/RX pins and does not provide the electrical interface or modem-control behaviour expected by the GM-10.

Look for a real RS-232 adapter with a DB9 connector and DTR support.

## Permissions

On Debian, serial devices are normally accessible through the `dialout` group. The supplied systemd unit runs:

```ini
User=gm10
Group=gm10
SupplementaryGroups=dialout
```

Verify access with:

```bash
id gm10
ls -l /dev/ttyS0 /dev/ttyUSB0 2>/dev/null
```

## Device settling

Real GM-10 testing showed a serial transient immediately after DTR powers the detector. `gm10d` handles this as:

```text
open/configure TTY
       |
       v
assert DTR
       |
       v
wait startup_settle_seconds (default 2)
       |
       v
tcflush(TCIFLUSH)
       |
       v
begin acquisition
```

Startup activity is intentionally discarded rather than interpreted as radiation events.
