# gm4lin 1.2.15 hardware notes

`gm10d` was designed with the original GPL `gm4lin 1.2.15` source as the hardware reference rather than relying on later third-party descriptions.

## Serial configuration

From `gm4lin`:

- `serial.h`: `SERIAL_BAUD_RATE B57600`
- `serial.h`: `SERIAL_WORD_LENGTH CS8`
- `serial.h`: no parity
- `serial.h`: one stop bit
- `serial.c:set_port()`: enables `CLOCAL | CREAD`
- `serial.c:set_port()`: disables canonical input/echo/signals

`gm10d` uses raw termios mode and explicitly configures 57600 8N1 with software and hardware flow control disabled.

## Detector power

`gm4lin`'s `dtr_up()` comment explicitly states that DTR is asserted **to provide power to the probe**. `start_probe()` configures the serial port and then raises DTR. `stop_probe()` lowers DTR before closing the port.

`gm10d` reproduces that lifecycle with `TIOCMBIS/TIOCMBIC`, which changes DTR without modifying unrelated modem-control bits.

## Particle event counting

The historical activity loop uses `select()` to detect readability. When a descriptor is readable it increments the probe value once and calls `clear_port()`, which reads up to eight bytes.

That means multiple bytes already queued in the UART may be consumed after only one increment. At low background rates this may rarely matter, but it can undercount when events arrive close together.

`gm10d` instead drains the serial input and increments `pulses_total` by the actual number of bytes returned by `read()`. This preserves the raw event stream and avoids the old readiness-event/count conflation.

## Historical sensitivity constants

`gm4lin.h` contains:

```c
#define SENSITIVITY_GM10_CO60 15
#define SENSITIVITY_GM10_CS137 21
```

and describes sensitivity in `cps/mR/h`. Version 0.1 of `gm10d` intentionally does not apply these constants automatically. Raw counts/CPM are exported first; calibrated exposure/dose estimates can be added later as an explicit optional feature.

## Historical 10-second reporting

`gm4lin` defaults to a 10-second activity sampling interval and uses:

```c
#define events2cpm (60.0/ACTIVITY_DATA_WRITE_SAMPLING)
```

so the displayed CPM is the 10-second count multiplied by six. `gm10d` exports this as `gm10_cpm_10s` for comparison, while `gm10_cpm` is the actual event count in the preceding rolling 60 seconds.

## gm10d startup-settle behaviour (0.1.1)

Real GM-10 testing showed a burst of serial bytes immediately after DTR powers the detector. Those bytes inflated the first rolling minute and cumulative pulse count, then disappeared from the 60-second window. `gm10d` therefore deliberately differs from gm4lin at startup: after asserting DTR it waits for `startup_settle_seconds` (default 2 seconds), drains and counts those bytes as diagnostics, then begins radiation acquisition. The discarded bytes are exported as `gm10_startup_discarded_bytes_total` and never enter `gm10_pulses_total`.
