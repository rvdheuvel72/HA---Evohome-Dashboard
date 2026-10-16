# RAMSES Arduino Gateway v0.4.0-beta.5 — ESP32-WROOM-32E

**Experimental diagnostic release — the module marked 433M is unchanged.** Neither TX method has been physically bench-verified for correct 868.3 MHz radiated output, channel occupancy or RF compliance.

### One-shot A/B test
- Added a **single-request transmit method selector**, **original asynchronous UART/GDO0** versus **reference-style CC1101 FIFO fixed length with UART 8N1 framing**. Each click transmits no more than one ID25 `RQ 3220` request. The methods are not run simultaneously or automatically in sequence.
- Added per-method attempt/local completion/Read-Ack/timeout counters and full TX telemetry via `GET /api/v1/opentherm/one-shot`: chosen method, reason codes, radio MARCSTATE, TX FIFO byte state, bytes sent, transmit time, PA value and readback, and RX restoration.
- Both transmit methods now explicitly program `PATABLE[0]=0x26` (TI 868MHz nominal -10dBm); beta.4 did not explicitly program PA. This is *not* a certified output power with an unmatched 433M RF network.
- FIFO method uses a prebuffered 55-byte FIFO frame (44 UART bytes plus 8N1 framing), **fixed length** (no FIFO underflow streaming), short timeout, and **high-impedance GDO0** to avoid colliding with the ESP32 UART TX wiring.
- Keeps the beta.4 manual consent, authenticated API, verified R8810A/source IDs, one-shot-only experimental 433M TX allowance, 60-second cooldown, no polling, no scanning, no write commands. All firmware changes require P1/P2 review and verified trusted build.

### Manual test plan
1. Flash new beta.5 using Arduino IDE, preserving your current GatewaySecrets.h / saved NVS settings. Verify `version: 0.4.0-beta.5` and `radio_verified: true`.
2. In Diagnostics, open **Experimental one-request read test**. Choose **Original asynchronous UART / GDO0**. Confirm the test and transmit **once**. Observe `/api/v1/opentherm/one-shot` after about 3s; write down TX phase, PA value, UART Read-Ack/timeout, and complete diagnostics.
3. At least 60 seconds later, choose **Reference-style CC1101 FIFO (fixed 44 bytes)**, confirm, and transmit **once**. Observe same endpoint. An OpenTherm `read_ack` proves the bridge responded to this unique virtual RF source address; `local_complete` only proves the local controller sequence completed.
4. Do not enable auto-poll, scan all IDs or generic read-request transmission. If the CC1101 reports TX underflow, PA mismatch, RX recovery error, or changed network behavior, stop testing and share diagnostics.

Reference: TI CC1101 data sheet PA table and FIFO operation; IndaloTech/ramses_esp 868.3 MHz Manchester encoding and CC1101 TX FIFO method. The implementation intentionally uses fixed FIFO length for fail-closed behaviour.
