# RAMSES Arduino Gateway v0.4.0-beta.25 — adaptive slow-SPI test after RX startup

## Verified M5Stack M146 receive-start evidence
At 1 MHz SPI clock the beta.24 logs show:
- After programming, `part=00 ver=14 io2=0D pkt=31 freq=21656A`.
- Immediately after `SRX`, `part=00 ver=14 marc=08 io2=0D pkt=32 hdr=40/40/40`, proving the SPI read path is functional during radio calibration.
- Approximately 16 milliseconds later, **every** register-data byte and command header status returns `0x1F`, including both chip ID registers, while RAMSES `valid=0`.

`0x1F` could be a genuine SPI **chip-status byte** (RX state and >=15 FIFO bytes), but it should not replace all *register-data* bytes. This can arise from misframed commands or an electrical SPI issue. The radio is not yet verified receiving.

## Candidate software-side recovery and test
- Retain the established 1 MHz SPI clock for reset, ID and all radio configuration.
- **Only if full register readback fails after SRX**, temporarily run *read-only* SPI audits at **125 kHz**. No new RF strobe, radio reset, register write, pairing or transmission is issued.
- Record `SPI-slow1`, `SPI-slow2`, and `SPI-clock RX comparison ... accepted=...` in the startup serial/web event log.
- Retain 125 kHz for later SPI operations **only if both independent audits** correctly read chip IDs, IOCFG2, PKTCTRL0, MDMCFG2 and all FREQ registers. The RX state and chip-status byte may vary without invalidating a healthy configuration.
- Otherwise revert to 1 MHz and preserve the failed slow-rate snapshots as evidence. If even 125 kHz returns uniform `0x1F`, the fault is not simply SPI clock speed.
- Host-native unit tests validate all combinations and avoid falsely accepting a single transient good sample.
- Existing read-only `/api/v1/radio/spi-audit`, `/api/v1/debug/logs` and `/api/v1/radio/uart-probe` remain available.

**This is an unverified physical-hardware hypothesis.** Running the ESP32 CI proves source safety and compilation, not recovery of RAMSES RF reception. Do not enable RF read requests or pairing until a valid receive packet has been confirmed.

Arduino IDE/source ZIP: `RAMSES-Arduino-Gateway-v0.4.0-beta.25-ESP32-WROOM-32E.zip`.
