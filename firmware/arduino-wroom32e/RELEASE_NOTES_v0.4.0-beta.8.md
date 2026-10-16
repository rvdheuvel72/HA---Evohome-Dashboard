# RAMSES Arduino Gateway v0.4.0-beta.8 — upstream-aligned FIFO RF waveform

## Root cause fixed
Beta.7 captured ordinary R8810A `3EF0` replies, but its manual primed 60-byte FIFO Read-Data ID25 request completed locally and timed out. An independent byte-level review of `IndaloTech/ramses_esp`'s `components/frame/frame.c` `tx_byte()` uncovered a concrete difference: upstream places the MARK/stop bit **before** each UART start bit, while beta.7 encoded START/data/STOP. This one-bit stream phase difference starts **directly after the four-byte BREAK/prime sequence**, and yields a first FIFO data byte of `AA` in the reference instead of beta.7's `55`.

## Changes
- The manually selected FIFO method now uses the exact upstream **MARK=1 / START=0 / eight data bits LSB first** bit sequence. Priming `00 FF 00 00` and final idle `FF` are retained, for **60 FIFO bytes total** in the CC1101 64-byte FIFO.
- A separate host-test model of the upstream 16-bit shift-register `tx_byte()` logic compares the **entire 60-byte stream** byte-for-byte, including 4-byte primer and trailer. Test also decodes exactly **one** valid OpenTherm ID25 RQ/3220 request, with incorrect lengths rejected.
- UART/GDO0 transmit code is **unchanged**. No heating writes, pairing, source-address impersonation, general RF TX, auto-retries, scans or polling were added. One-shot remains authentication protected, limited to R8810A, constrained to one ID25 Read-Data, 60-second cooldown and explicit user confirmation of RF identities and single-request authorization in the web interface. The separate physical-RF/module safety checkbox is only enforced for normal TX, which this experimental one-shot path forbids; no hardware-verification gate is enforced here.
- RX Debug ON/OFF, bridge traffic counters and the signed RSSI correction from beta.6 remain available.

## Physical and protocol limitations
A local CC1101 `local_complete` report does **not** demonstrate over-the-air output, legal emissions or remote receipt. The existing 387–464 MHz-marked CC1101 PCB/antenna is not measured for 868.3 MHz transmission. A paired Evohome controller receiving `3EF0` responses does not establish that an unpaired virtual `18:` gateway will be accepted for `3220`.

## Suggested controlled validation
Keep all current settings. Flash beta.8, wait for `bridge_seen=true`. Select **Reference FIFO (upstream bit-aligned, 60 RF bytes)**, manually authorize one read-only ID25 request, wait for the reported result, then collect `/api/v1/opentherm/one-shot` and `/api/v1/radio/diagnostics`. Do not enable normal TX or repeat requests outside the 60-second gate.

## Release policy
Protected ESP32 compile, source tests, HA CI and exact-head Codex P1/P2 gate are mandatory. Authenticated Codex usage-quota deferral is allowed only if all checks pass and zero unresolved P1/P2; log review debt for the next available run, including past issues #28, #30, #32, #34 and #36.
