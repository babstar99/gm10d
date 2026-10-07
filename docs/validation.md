# Real-hardware validation

`gm10d` was developed and tested with a real Black Cat Systems GM-10 rather than only simulated serial input.

The measurements below validate detector/interface behaviour and the acquisition pipeline. They are **not an absolute radiation calibration** and should not be used as a CPM-to-dose conversion.

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
- persistent `/dev/serial/by-id/...` device path

Initial extended observation:

- 56,916 seconds (~15.8 hours) uptime
- 14,371 cumulative events
- calculated long-run mean approximately 15.15 CPM
- zero serial read errors
- zero serial reconnects
- MQTT connected

The USB result was consistent with the native-UART background range, supporting the FTDI adapter as a long-term replacement for machines without physical RS-232 ports.

## Known-source response: Tc-99m, 2026-10-07

An unplanned real-world validation occurred when an independently known Tc-99m source entered the detector environment following a nuclear-medicine procedure.

Before the source was identified, the long-running GM-10 installation showed an otherwise stable background around 15 CPM followed by several statistically unusual excursions. The largest initial rolling 60-second value was approximately 245 CPM. Retrospective analysis of `gm10_pulses_total` confirmed that later elevated periods represented new counts rather than only persistence in the rolling CPM window.

After the source was identified, bringing it close to the GM-10 produced a repeatable response of approximately 500 CPM. This provided an independently known radioactive stimulus without changing the detector, serial hardware, daemon configuration, or acquisition algorithm.

### v0.1.4 serial-read diagnostics

Version 0.1.4 was installed before a deliberate close-proximity observation. It added serial-read diagnostics while preserving the existing rule that every received byte is counted as one GM-10 event.

A process-lifetime diagnostic snapshot after the close-proximity observation showed:

```text
gm10_pulses_total                     1641
gm10_max_events_1s                      21
gm10_serial_read_calls_total           1608
gm10_serial_bytes_total                1641
gm10_serial_multibyte_reads_total        30
gm10_serial_multibyte_bytes_total        63
gm10_serial_reads_1byte_total          1578
gm10_serial_reads_2_4bytes_total         30
gm10_serial_reads_5_16bytes_total         0
gm10_serial_reads_gt16bytes_total         0
gm10_serial_max_read_size_bytes           3
```

The current rolling CPM had already returned close to background by the time this snapshot was taken, but the process-lifetime diagnostics preserved the short high-rate behaviour.

Of 1,608 successful serial reads, 1,578 (98.1%) returned exactly one byte. The 30 multi-byte reads contained only 63 bytes in total, with a largest read size of three bytes. There were no reads of five bytes or more. The largest one-second event bucket contained 21 events.

This is strong evidence against the observed high-count response being caused by a small number of large serial garbage bursts. It also demonstrates why byte-accurate counting matters: under a genuine elevated radiation field, closely spaced detector events can be coalesced into two- or three-byte reads by the serial/kernel path. Counting only one event per readability notification, as historical `gm4lin` did before draining queued bytes, would undercount such events.

### What this validates

The Tc-99m observation supports all of the following:

- the GM-10 responds strongly to a known gamma-emitting source;
- `gm10d` preserves closely spaced detector events rather than collapsing them into one readiness event;
- the FTDI USB-to-RS232 path can carry elevated event rates without large read bursts, reported serial errors, or reconnects;
- the `/metrics` -> vmagent -> VictoriaMetrics -> Grafana pipeline preserves both short excursions and the cumulative event record;
- process-lifetime serial diagnostics can distinguish normal coalescing of nearby events from large serial bursts.

### What this does not validate

This observation is **not a detector calibration**. The source was not a certified calibration source, source-to-detector geometry was not fixed, administered activity was not recorded for this test, and biological clearance changes the source strength with time. No CPM-to-Bq, CPM-to-dose, or energy-response coefficient should be derived from these observations.

The result should therefore be treated as a known-source **functional and acquisition-path validation**, not an absolute sensitivity measurement.

## Integrations validated

The deployment has been exercised end-to-end with:

- systemd service supervision
- Mosquitto authentication and ACLs
- Home Assistant MQTT discovery and live entities
- a bridged Mosquitto topology
- remote vmagent scraping
- VictoriaMetrics stream aggregation
- Grafana CPM graphing

## Overall limitation

Observed CPM values are detector-, geometry-, spectrum-, shielding-, and environment-dependent. The validation above establishes software/interface consistency and demonstrated response to a known radioactive source; it does not establish a universal calibration standard.
