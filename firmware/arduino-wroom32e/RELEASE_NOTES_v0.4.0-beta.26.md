# RAMSES Arduino Gateway v0.4.0-beta.26 — bounded receive-path software bypass

## Why this is necessary

With M5Stack M146 CC1101 868 MHz hardware, the radio's SPI registers are readable after chip reset and configuration, and immediately after the SRX command while the chip is calibrating. Roughly 16 ms later, *every* register and the header status byte return `0x1F`, including PARTNUM and VERSION. Slowing the SPI clock from 1 MHz to 125 kHz did **not** recover valid register reads: two slow audits contained inconsistent invalid bytes. RAMSES sync and valid frames remain zero.

This build tests software paths that could bypass the failure without changing wiring, buying hardware, transmitting RF or pairing.

## Safeguarded changes

1. **Quiet RX startup.** After SRX, leave the CC1101 SPI interface undisturbed for 25 ms instead of issuing repeated status reads at 150 µs intervals during PLL calibration/settling. Then take one full read-only register audit.
2. **Existing slower-clock test.** If the first post-RX audit is still invalid, retain the previous dual 125 kHz read-only attempt; keep the slower clock only if both readings verify the complete config.
3. **True continuous SPI burst.** If both speeds fail, try reading CC1101 normal configuration registers 0x00..0x12 in one CSn-low burst, rather than a string of two-byte per-register accesses. Record `SPI-rxburst` with the actual config data, status byte and validation.
4. **One controlled IDLE-to-RX reentry.** Only after SPI failures, issue one SIDLE and read back the IDLE config with individual reads (`SPI-idle-check`) and a single long burst (`SPI-idle-burst`). Then **restore RX** through the existing SIDLE / IOCFG0 / PKTCTRL0 / SFRX / SRX receive-only sequence. After a 25 ms quiet period read the configuration once (`SPI-return`).
5. Emit `SPI-bypass sidle=... reentry=... idle_spi_ok=... return_spi_ok=...`. These are command-attempt/verified-register statuses, **not evidence of RF reception**.
6. **Bounded, not recurring.** The intrusive IDLE-to-RX test occurs once per radio initialization, only when all preceding readbacks fail. It is not triggered just by missing RAMSES frames. It does not invoke SRES, STX, new OpenTherm commands or pairing.

## Interpretation

- If quiet RX startup alone restores correct SPI and RAMSES frames, repeated SPI polling during radio PLL settling was a contributing issue.
- If normal reads fail but `SPI-rxburst ... ok=1`, SPI transfer framing/CSn is the likely direction for a production workaround; do not assume RF packets are recovered merely because configuration reads succeed.
- If `SPI-idle-check` works, but `SPI-return` fails, the failure tracks actual RX hardware state, and a safe software bypass may not exist. Power/GND/MISO/CSn electrical integrity during active RX becomes the leading target.
- If IDLE and RX both fail, CSn control, interface state and electrical reliability remain suspect.
- If `SPI-bypass ... return_spi_ok=1`, re-entering RX restored readable register configuration, though actual RAMSES reception must still be observed.

No physical hardware results have been collected for this version yet. An RX-only software workaround cannot manufacture a GDO2 data waveform; `valid_frames > 0` is still the proof of successful RAMSES reception.

Arduino IDE/source and compiled firmware ZIP: `RAMSES-Arduino-Gateway-v0.4.0-beta.26-ESP32-WROOM-32E.zip`.
