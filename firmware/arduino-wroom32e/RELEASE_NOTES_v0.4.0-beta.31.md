# RAMSES Arduino Gateway v0.4.0-beta.31 — DTS92 compatibility after successful RF pairing

## Breakthrough observed

On 2026-10-25 16:30:55 CET, a real Evohome controller `01:245914`
responded to our ESP32 virtual thermostat `12:160001` with
`W --- 01:245914 12:160001 --:------ 1FC9 006 03230907C09A`.
This confirms that the thermostat offer was received over the air
and that Evohome offered **zone index 03**. The ESP32 locally completed
the 46-byte confirmation, but its delivery was not independently verified.
Original Evohome ↔ R8810A OpenTherm traffic continued unchanged.

## Real type-12 compatibility corrections

Upstream `ramses-rf/ramses_rf` captures for physical `12:010740`
in `tests/tests_rf/data_driven/parsers/code_1fc9.log` show:
- Controller acceptance `W ... 1FC9 006 08230906368E`, with zone 08.
- **12: confirmation `I ... 1FC9 006 0023093029F4`**,
  with *binding index 00*, not 08.

Upstream `code_30c9.log` shows a real DTS92 spontaneous announcement:
`I --- 12:010740 --:------ 12:010740 30C9 003 000785`.

The emulator now generates this type-12-specific confirmation format
and reports `30C9` as a self-addressed broadcast, with payload index 00.
The associated Evohome zone index 03 is still retained for UI and
pairing-state checks, but not inserted into type-12 spontaneous frames.
Native C++ tests validate the captured frame structure and checksum.

All beta.30 independent virtual controller `01:`, virtual thermostat
`12:`, manual local-only stop/forget controls, authenticated external
sensor input, stale-reading safeguards, and RX diagnostics remain.

A successful physical pairing and correct display of transmitted room
temperature have not yet been verified after this frame-format correction.
Do not mistake local CC1101 TX completion for receipt of a confirmation
or 30C9 temperature report. These must be checked on the actual Evohome.
