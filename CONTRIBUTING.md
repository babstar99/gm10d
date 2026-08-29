# Contributing

Contributions are welcome, especially for additional real-hardware validation, serial adapters, GM-10/GM-45 hardware notes, MQTT/Home Assistant compatibility, and Linux packaging.

## Design principles

Please preserve the project's main goals:

- small native C implementation
- minimal well-maintained dependencies
- no Python/pip runtime stack
- raw event counts remain authoritative
- serial acquisition must not depend on MQTT or metrics availability
- avoid adding a dependency when a small, robust POSIX implementation is sufficient
- prefer mature distro-packaged libraries when implementing a protocol correctly would otherwise add risk

## Build

On Debian/Ubuntu:

```bash
sudo apt install build-essential libmosquitto-dev
make
make check
```

Before submitting a change:

```bash
make clean
make CFLAGS='-O2 -g -Werror'
make check CFLAGS='-O2 -g -Werror'
```

## Code style

- C11 / POSIX/Linux APIs
- keep functions small and explicit
- check system-call and library errors
- no busy polling
- use monotonic time for rate windows and retry scheduling
- avoid silently changing measurement semantics

## New hardware reports

Please include:

- adapter/vendor/model
- USB VID:PID if applicable
- kernel driver and TTY path
- whether DTR successfully powers the detector
- duration of test
- serial read errors/reconnects
- approximate long-run CPM compared with a known-good interface, if available

Do not publish unique serial numbers unless they are intentionally public.
