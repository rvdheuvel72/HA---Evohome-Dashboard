# RAMSES Arduino Gateway v0.4.0-beta.12 — passive R8810A controller discovery and corrected binding instructions

## Fixes
- A received, checksum-validated **`RP 10:R8810A → 01:Evohome 3EF0`** now identifies the existing Evohome controller, even if the preceding **`RQ 01:Evohome → 10:R8810A`** was missed by the CC1101. This improves visibility of `existing_controller_id` and pairing preflight after reboot. It does **not** initiate RF transmission or establish a binding.
- Controller inference is tightly restricted to source `10:` matching the configured R8810A, a direct `01:` destination, verb `RP`, opcode `3EF0`, and valid address slots. It preserves the first observed controller identity rather than replacing it with a later virtual ID.
- Added host-native regression tests for bridge reply, original controller RQ, unrelated bridge, unsolicited `1FD4`, wrong destination type, and malformed address layouts.
- Corrected the earlier beta.11 **manual error**: the manufacturer R8810A1018 installation guide explicitly describes a **5-second press** and red LED 0.5s ON/0.5s OFF to enter binding mode, **without instructing a preceding 15-second clear** in that procedure. Do not erase any existing Evohome binding.
- The web diagnostics clarify that clicking **Send one pairing offer** merely queues an ESP32 RF attempt. It does **not** physically switch an R8810A into bind mode; the physical button must be deliberately operated if you choose to experiment.

## Limits and safety
- The cited device manual documents binding a CM957-type wireless thermostat. It does **not** demonstrate additional virtual `01:` controllers can bind to an already-paired Evohome R8810A without disturbing normal operation.
- This remains an **experimental** additional-controller implementation. No pairing is performed automatically; there is no write command or binding reset.
- Keep the existing Evohome heating operational. Verify the `/api/v1/opentherm/pairing` `state`, `offer_accepted`, `tx_phase`, and `verified_reply` separately; queued does not mean transmitted, and locally transmitted does not mean acknowledged.

Manufacturer reference: https://www.verwarmenperkamer.nl/productblad/honeywell-evohome-opentherm-module-r8810a1018-installatie-handleiding-instructies.pdf — page 2, “Binding a new wireless component”.
