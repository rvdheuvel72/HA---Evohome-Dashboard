# RAMSES II Gateway v0.4.0-beta.4 — ESP32-WROOM-32E

**Experimental firmware.** Builds and protocol tests are automated; actual CC1101 transmissions and emissions at 868.3 MHz on a PCB marked `433M` have not been physically certified.

## New: single-request diagnostic using the EXISTING 433M board
- Standalone Diagnostics button sends **exactly one** manually approved `RQ 3220` OpenTherm **Read-Data ID 25** (boiler flow temperature) to the configured R8810A; no retransmits, writes, scans, or automatic polling.
- Supports an explicit cross-band experimental request at 868300 kHz while retaining the *true physical board marking* of 433 MHz. Existing 868-band normal scans remain separately guarded and disabled by default.
- Requires strong web authentication, per-click consent, configured bridge and unique non-reserved virtual 18: RF source IDs, an observed bridge, no source-ID collision, frequency restriction and a 60-second attempt cooldown. Times out after 1.8 seconds.
- A separate `GET /api/v1/opentherm/one-shot` status API reports readiness, ID25 status, cooldown, raw response and decoded flow temperature when a Read-Ack arrives.
- Existing 433 MHz RX support, RF pinout, MQTT, Home Assistant integration and normal RF scan controls are unchanged.

## Hardware test instructions
1. Safely connect the previously tested ESP32/CC1101 V2.0 and use firmware `v0.4.0-beta.4`. Confirm real RF `radio_detected` and valid RAMSES packets first.
2. Under Settings select real **433 MHz PCB**, **868300 kHz** and experimental cross-band RX. Enter only your confirmed R8810A decimal RF ID (packet `10:048994` would be `48994` if verified) and choose a unique **unused** decimal virtual 18: source ID between 1 and 262142 other than 730. Do not enter the MQTT gateway ID as the virtual RF address if it exceeds the valid address range.
3. Leave `Enable RF read-request transmission`, `Automatic read polling` and `Scan all 256` unchecked. Save and restart.
4. Wait for gateway Diagnostics to show the bridge as observed, expand the **Experimental one-request** section, read the caution, manually tick the consent and click once. No request occurs just by opening the page.
5. Inspect `/api/v1/opentherm/one-shot`: `read_ack` confirms a correlated reply; `timeout` or `tx_failed` is not proof that the boiler lacks OpenTherm functionality. Wait at least 60 seconds before another manually approved request.

**Security and radio caution:** This diagnostic transmits only a read request, but it still adds RF traffic to the existing heating control network. The CC1101 PCB's on-air emissions at 868 MHz may differ from an RF-matched design; local legal compliance and lack of interference have **not** been validated. Keep the gateway on a trusted LAN because HTTP Basic authentication is unencrypted. The user should be physically present for the first test.

The released ZIP and SHA256 are immutable GitHub Release assets. All P1/P2 findings must be fixed or approved quota deferral recorded with no unresolved P1/P2 before release.
