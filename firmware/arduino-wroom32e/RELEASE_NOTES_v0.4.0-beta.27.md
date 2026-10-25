# RAMSES Arduino Gateway v0.4.0-beta.27 — unpaired 868 MHz OpenTherm ID25 one-shot

## Scope
This release preserves the RX initialization and stable RAMSES reception fixes from
beta.26, and expands the **manual**, **read-only** OpenTherm ID25 test to a
genuine 868 MHz CC1101 module before additional-controller pairing.

- Allow a one-shot `RQ/3220` Read-Data ID25 from a unique, unobserved
  `18:xxxxxx` gateway RF identity on exactly **868.300 MHz / 868 MHz module**.
- Preserve the separate **verified bound `01:`** source path; never impersonate
  the existing Evohome controller.
- Preserve the explicitly opted-in legacy 433 MHz experimental cross-band path.
- Retain strong web authentication, manual confirmation, configured/observed
  R8810A, source collision checks, automatic RF TX disabled, full scan disabled,
  auto-poll disabled, one request per click, 1.8-second response deadline,
  60-second cooldown and RX restore.
- Add native regression tests for eligible unpaired 868 MHz operation,
  source collisions, reserved IDs, invalid module bands and mismatched frequencies.
- Change the Codex P1/P2 gate workflow runner to self-hosted Linux.

## Validation
The exact-head self-hosted firmware test/Arduino compile, trusted self-hosted
compile, Codex P1/P2 review and release-readiness contract must pass before
publishing. Do not treat a locally completed TX as evidence of RF delivery.

## Hardware test
From the authenticated gateway web UI, configure bridge `10:048994` as
observed, a genuinely unused `18:` RF source, 868.300 MHz and hardware
band 868. Keep automatic TX, scan and auto-poll switched **off**. Use the
**Manual OpenTherm ID25 read test** panel for a single explicitly authorized
read and inspect the resulting status/diagnostics for a correlated `3220`
reply. A timeout alone does not prove defective radio hardware.

**Installing firmware makes no RF transmission or pairing request on its own.**
