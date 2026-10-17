# RAMSES Arduino Gateway v0.4.0-beta.11 — explain pairing readiness and protect existing Evohome binding

## Fixes
- Fix a practical beta.10 usability problem: the **Send one pairing offer** button remained disabled with no obvious explanation. The Diagnostics tab now shows specific preflight reasons and how to address them.
- `GET /api/v1/opentherm/pairing` now includes `radio_ready`, `block_reason` and `ready` based on the actual pairing start preflight. Reasons distinguish unobserved R8810A, unobserved original Evohome controller, RF not initialized, configuration missing, active scans/polling, virtual-address coverage exhausted, in-progress one-shot, existing pairing, and cooldown.
- The web panel now clearly identifies a missing virtual controller ID and unconfirmed consent as blockers. HTTP errors and inaccessible pairing status are shown visibly.
- Correct the physical device instructions: **the official Evohome R8810A installation procedure clears previous bindings with 15 seconds BEFORE its 5-second bind press.** A 5-second press may not enter binding mode on an already-bound bridge. The ESP32 firmware cannot remotely put the bridge into binding mode. **Do not reset or clear a working Evohome/boiler binding** to test this experimental virtual controller.
- All transmitter safety controls and the 1FC9/3220 binding experiment are unchanged. There is no automatic RF transmission, background polling, heating override, or factory reset.

## Limitations and important safety information
This release **does not establish that the R8810A accepts a second virtual 01: controller without erasing the existing Evohome binding**. If the bridge does not display its expected slow red LED blink, stop the attempt and retain the normal Evohome connection. The status may show software READY while the *physical bridge* is not in bind mode; software READY only means the local ESP32 preflight has passed. Never mistake local TX success for over-the-air pairing verification. The current experimental binding identity is in-memory only and is lost after reboot.

Manufacturer reference: Honeywell/Resideo Evohome Installation Guide, OpenTherm Bridge (R8810), page 21, plus R8810A device installation guide.
