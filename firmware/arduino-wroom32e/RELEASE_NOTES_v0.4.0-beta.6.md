# RAMSES Arduino Gateway 0.4.0-beta.6 — ESP32-WROOM-32E

### RX diagnostics and on/off switch
- New **Passive radio receive diagnostics** panel in the web Diagnostics page with a checkbox for **RX Debug ON/OFF** (15-second summary log, off on restart, changes applied immediately).
- New authenticated `GET /api/v1/radio/diagnostics` passive JSON counter endpoint and constrained `POST` toggle using `X-Gateway-Config: radio-debug` and `enabled=1/0`; no NVS modifications, restart, RF transmit, RF auto-poll or packet filter changes.
- Counts total valid messages **from** the R8810A separately from controller requests **to** R8810A, including `3EF0` and `3220` from the bridge.
- Separately counts invalid Manchester symbols, bad checksums, structural invalidity, incomplete/truncated frames, size excess, idle timeouts and format failure. Broken RF packets cannot reliably be assigned to a sender.
- Corrected CC1101 signed RSSI handling. RSSI sampled after a packet remains only indicative.
- Cleaned up Arduino compiler warnings for volatile counters, formatting type mismatches and potentially truncated UTC timestamp.
- Maintains beta.5 UART/FIFO one-shot comparison, existing 433M module setting, explicit one-shot RF safety guards and 60s cooldown. No changes to heating operation.

### Verification
Host unit tests cover valid/invalid frames, reason classifications, virtual gateway independence and R8810A request/reply distinction, plus RSSI signed values. Protected ESP32-WROOM-32E Arduino build and full repository CI are required before publishing.

### Use
Flash beta.6 and retain NVS settings. In web Diagnostics toggle RX Debug ON, allow heating/controller traffic for several minutes, then open `/api/v1/radio/diagnostics` and `/api/v1/debug/download`. Debug can be turned OFF immediately. To evaluate R8810A receive problems, check `requests_to_bridge` rising against `messages_from_bridge` and rejected errors; do not disable the bridge safety gate based on counts alone.
