# RAMSES Arduino Gateway v0.4.0-beta.22 — CC1101 SPI register-read audit

## Why this exists
On the M5Stack M146 868MHz module, beta.21 still detects CC1101 silicon
(`PARTNUM=0x00, VERSION=0x14`) but repeatedly reports `MARCSTATE=31`,
`IOCFG2=31` (expected 13) and `PKTCTRL0=31` (expected 50).
Both the board pin map and the user's stated connections match the firmware:
ESP32 GPIO23→MOSI, 19→MISO, 18→SCK, 5→CSN, 27→GDO2, 26→GDO0;
module powered from 5V and common GND. Their physical electrical continuity
and level quality have **not** been independently measured.

## Firmware diagnostic changes
- New authenticated read-only endpoint `GET /api/v1/radio/spi-audit`.
  Captures CC1101 PARTNUM, VERSION, **unmasked** MARCSTATE, two independent
  samples each of IOCFG2 and PKTCTRL0, MDMCFG2 and FREQ2/FREQ1/FREQ0.
- Compares frequency readback to the synthesizer word actually programmed.
  Flags identical data returned from multiple unrelated registers as
  `uniform_spi_readback_suspected`; a common value is evidence of an
  unreliable SPI result, not proof of a particular defective connection.
- Snapshots are collected only by the RF task on boot and at most once every
  30 seconds when not decoding a packet. HTTP reads cached data; it does
  not access SPI, reset the chip, program a register, or transmit RF.
- WSL-Ubuntu lab diagnostics now capture this endpoint and pass its results
  to read-only Codex analysis, tolerating older beta versions without it.
- Host-native unit tests validate good register readback vs the observed
  0x1F pattern; full Arduino-ESP32 build is required.

## Interpretation
- `uniform_spi_readback_suspected`: troubleshoot SPI data and select
  integrity; a working chip identifier does **not** prove configuration
  registers are readable.
- `register_configuration_mismatch`: SPI responds, but the sampled radio
  configuration differs from programmed values.
- `config_readback_matches`: sampled registers match; verify CC1101
  receive-state and GDO2 waveform separately.
- This release does **not** prove RAMSES packets will be received. No new
  RF TX, binding, pairing or OpenTherm boiler-writing behavior is enabled.

Arduino IDE/source and firmware ZIP:
`RAMSES-Arduino-Gateway-v0.4.0-beta.22-ESP32-WROOM-32E.zip`.
