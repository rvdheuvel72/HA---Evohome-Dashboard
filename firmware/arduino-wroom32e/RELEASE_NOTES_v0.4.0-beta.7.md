# RAMSES Arduino Gateway 0.4.0-beta.7 — guarded FIFO priming test

## Why
The beta.6 experimental one-shot transmitted a locally completed 44-byte UART / 55-byte FIFO Read-Data ID25 request but received no matching reply, although the R8810A sent other valid `3EF0` packets. The reference upstream `IndaloTech/ramses_esp` uses a short FIFO priming/break sequence and trailing idle MARK that our earlier fixed-length FIFO path omitted.

## Firmware changes
- Updated reference FIFO waveform: **4-byte `00 FF 00 00` startup/break + 55-byte 8N1 Manchester-coded RAMSES frame + final `FF` idle MARK = exactly 60 bytes** (fits a single CC1101 64-byte FIFO; no streaming or ISR refill).
- The original asynchronous UART/GDO0 TX mode remains separately selectable and **unchanged**.
- No new OpenTherm IDs, generic RF TX, writes, pairing/binding, automatic retries, background polling, normal TX or changes to the receiver's address handling. No commands impersonate the paired Evohome controller.
- The one-shot button is manual and authenticated, still restricted to a single 3220 Read-Data ID25 to configured R8810A, with verified bridge identity and 60-second cooldown.
- TX diagnostics now label FIFO as `fifo_reference_primed_60`, with `tx_expected_bytes=60` and a detailed local outcome. Local success does **not** prove RF over-the-air delivery.
- Retains beta.6 passive RX Debug switch, signed RSSI fix and rejection counters.

## Verification
The unit suite checks the four primer bytes, 55-byte exact 8N1 payload, final idle byte, 60-byte FIFO bound, negative lengths, and reconstruction of exactly **one** valid Read-Data 3220 request. The Arduino ESP32-WROOM-32E build and protected CI/review gating are required before release.

## Caution
The original 433 MHz-marked 387–464 MHz RF PCB and antenna are not characterized at 868.3 MHz. The fixed nominal CC1101 PA table value does not certify RF output or compliance. Do not assume the bridge received the request; test one manual request only and compare `/api/v1/opentherm/one-shot` with `/api/v1/radio/diagnostics`. The R8810A's acceptance of a new virtual `18:` sender remains unverified.

Codex review may be deferred only after an authenticated quota notice, green trusted CI and zero unresolved P1/P2; track review debt until quota is restored.
