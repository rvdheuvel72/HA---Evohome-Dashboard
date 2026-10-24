# RAMSES Arduino Gateway v0.4.0-beta.23 — automatic boot-time SPI timeline

This release answers the repeated M5Stack M146 SPI readback anomaly:

- At boot the gateway reports CC1101 `PARTNUM=0x00`, `VERSION=0x14`, but by 60 seconds all audited registers return `0x1F`.
- The old periodic audit only retained its latest snapshot; a browser could miss the earliest state.
- Beta.23 captures **three initial, raw, read-only register snapshots inside radio initialization** before Wi-Fi/HTTP access is required:
  - `SPI-pre`: after CC1101 reset/ID check, before configuration writes
  - `SPI-cfg`: after all modem and frequency register writes, before SRX
  - `SPI-srx`: immediately following the RX start sequence and any bounded retry
- Each phase is written once to the existing timestamped web diagnostics log and serial console. Lines include uptime in milliseconds, PARTNUM, VERSION, **unmasked** MARCSTATE, repeated IOCFG2/PKTCTRL0, MDMCFG2, FREQ2/FREQ1/FREQ0, expected frequency word, and a uniform-readback flag (`uni=1`). No register resets or transmissions are triggered by these snapshots.
- The first runtime comparison (approximately 30 seconds after initialization) is also logged as `SPI-runtime`, with subsequent log entries only if the sampled registers change.
- Diagnostics remain accessible from `/api/v1/debug/logs` or `/api/v1/debug/download`; a power-on record is available without opening `/api/v1/radio/spi-audit` within a short startup window.

**No claim of repaired RF reception** is made. These read-only traces can show whether the repeated `0x1F` response appears before programming, after programming, after the RX command, or only at runtime, helping distinguish SPI handling, power/signal integrity, and firmware sequence issues. The debug log uses a bounded ring, so older events eventually roll off; save the startup log soon after boot.

Arduino IDE/source and firmware ZIP: `RAMSES-Arduino-Gateway-v0.4.0-beta.23-ESP32-WROOM-32E.zip`.

RF TX, pairing and boiler control are unchanged and disabled by default.
