# RAMSES Arduino Gateway v0.4.0-beta.13 — stronger local TX verification

## Changes
- The manually authorized R8810A `1FC9` binding attempt now captures **live** CC1101 register readbacks for FREQ2/1/0 (vs computed frequency), PKTCTRL0 (asynchronous transmit mode), IOCFG0 (GDO0 configured as asynchronous data input), MDMCFG4/3/2 (modem rate/configuration), and PATABLE readback.
- Records MARCSTATE before/during TX, immediately **after UART flush while the transmitter should still be in TX**, and after the radio is restored to a stable RX state; samples RX for up to 5ms without extra TX strobes.
- Records UART expected vs written bytes, elapsed UART time and conservative minimum 38400-baud 8N1 wire time; detects implausibly short flush and other local inconsistencies.
- `GET /api/v1/opentherm/pairing` adds `tx_step`, `tx_local_verified`, `tx_local_verification_reason`, all relevant register/state readbacks and timestamps. The on-device pairing panel displays the verdict and why it did/did not pass.
- Preserves existing `tx_phase`, `tx_bytes`, manual authorization, 60-second cooldown, single-attempt 1FC9 binding state machine, read-only verification and native heating pairing. Adds no extra radio transmission or automatic retry.
- New host-native positive and negative regression tests exercise each local-verification failure reason.

## Read the result correctly
`tx_local_verified: true` means **only** that the ESP32 UART/CC1101 internal registers, MARCSTATE, timing and RX restore checks passed. It **does not prove** that GPIO26 is physically wired to GDO0, that RF energy was radiated, that the R8810A decoded the offer, or that binding succeeded. Only `offer_accepted` and `verified_reply` give over-the-air/R8810A protocol evidence.

## Safety
This is a diagnostic-only feature on the existing hardware. Do not reset, erase or remove existing R8810A ↔ Evohome binding. Continue to verify native Evohome boiler control separately.
