# RAMSES Arduino Gateway v0.4.0-beta.10 — experimental R8810A manual pairing

## Experimental feature
- New **Diagnostics → Experimental R8810A pairing** panel with a candidate virtual controller `01:NNNNNN` and a deliberate operator confirmation.
- Uses a single `1FC9` binding offer, waits for a response from the **configured R8810A**, sends one confirmation only when the reply matches, and transmits **one** `3220` OpenTherm Read-Data ID25 verification request from that virtual controller identity.
- The status endpoint `GET /api/v1/opentherm/pairing` reports queued, waiting, confirmed-but-unverified, verified-reply, timeout, failed or address-conflict states. `POST` needs HTTP authentication, a strong non-default password, `X-Gateway-Config: opentherm-pairing`, the exact confirmation token and a distinct numeric `virtual_controller_id`.
- Verified `01:` identity is used by subsequent **manually authorized, read-only** OpenTherm queries for the remainder of the boot session. It is intentionally **not persisted**, and no pairing attempt occurs at boot.
- The existing physical Evohome controller and its bound R8810A remain untouched in firmware: there is no binding reset command, no raw packet injection, no heating demand or OpenTherm Write-Data operation.
- Cannot start until R8810A and an existing `01:` controller addressing it have been observed, while scan/poll/automatic TX are OFF and a 60-second per-attempt cooldown has expired. No automatic retries.
- Unit tests cover encoded 1FC9 offer and confirmation, OpenTherm verification source identity, expected addressing and acknowledgments, timeout, wrong-device reply rejection and virtual-ID collision detection.
- Pairing fails closed if 12 unique `01:` controller identities have already been observed: the ESP32 cannot guarantee collision-free identity selection once its bounded table is full. The pairing status JSON reports coverage exhaustion and the UI disables the button. New host-native tests enforce this safeguard.
- RX replies are refused at or after their 1FC9/3220 deadlines, including while the radio task is delayed and across `millis()` rollover.

## Important limitations
**Experimental, not physical-hardware validated.** The capability triplets in the 1FC9 offer are based on public RAMSES controller/relay binding captures; R8810A acceptance of an additional virtual `01:` source has not been demonstrated. Sending the final 1FC9 confirmation does **not** prove pairing. A correctly addressed 3220 response is required for `verified_read_reply`, and even that does not guarantee continued binding or operational boiler control. The ESP32 must never replace the existing `01:` Evohome ID. Use **only the 5-second R8810A bind-mode press**, **never the 15-second erase**, and immediately check that normal Evohome heating still controls the boiler after any experiment. Do not use on a heating-critical installation without monitoring and a recovery procedure.

Code/reference sources: Honeywell R8810A installation instructions and public RAMSES II `1FC9` controller/relay examples. R8810A-specific offer acceptance remains to be captured and verified.