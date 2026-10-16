# RAMSES Arduino Gateway v0.4.0-beta.9 — on-device RX packet capture

## New operator controls
- **Diagnostics → Extended RAMSES packet capture**: Start, Stop, Clear and Download TSV on the ESP32 web UI, without reboot, RF transmitter enablement, NVS writes or interfering with heating control.
- Dedicated *bounded* 192-event RAM session (approx. 46 KiB reserved DRAM); stores all valid decoder frames and rejected Manchester/structure/checksum candidates seen while active. Automatically stops at capacity **without overwriting earlier evidence**; Start and Clear discard unsaved history, reboot loses capture.
- Capture data is written on the radio task before the existing 12-entry web/MQTT queue; this prevents queue saturation or the 12-entry recent-history rollover from discarding captured evidence. The ordinary `/api/v1/packets` last-12 endpoint remains unchanged.
- Valid events include complete HGI-style RAMSES line (source, destination, verb, opcode, payload), indicative RSSI, local acquisition timestamp using `Europe/Amsterdam` if configured, UTC epoch milliseconds, uptime and sequence; rejected events include rejection reason and failing UART byte, **no trustworthy sender**.
- Authenticated `GET /api/v1/capture/status`, `POST /api/v1/capture/action` with `X-Gateway-Config: packet-capture` and `action=start|stop|clear`, `GET /api/v1/capture/download` streamed TSV.
- Status exposes captured decoded/rejected, full/active, UART bytes and decoder sync deltas, dropped packets in web/MQTT queue since capture start. No raw SDR bitstream or missed-RF visualization is claimed.
- Added host tests for session start/stop/reset, no packet loss before full, bounded size, timestamp storage and full/overflow state.
- Preserves prior beta.8 upstream-corrected reference FIFO TX waveform, experimental single-request ID25 safety gates, RSSI diagnostics and RX Debug switch.

## Why gaps occur
The old packet-view endpoint retains *only the latest 12 valid frames*, so missing a `3EF0 RQ` in a later snapshot can simply mean rollover. The frame decoder rejects invalid Manchester/structure/checksum candidates and displays none of them in `/api/v1/packets`; capture now records those reasons, but cannot attribute them to a particular sender. CC1101's periodic receive recalibration briefly interrupts RF listening, the antenna/matching may be imperfect, and the UART input can overflow or have noise. The `dropped_frames` counter shows actual web/MQTT queue drops; zeros there are encouraging but not proof that no RF packet was missed.

## Safety and release
Every change requires protected Arduino ESP32-WROOM compile, host tests, HA CI and exact-head Codex P1/P2 review or authenticated quota deferral with no unresolved P1/P2 and documented review debt. Neither firmware nor a download claims perfect RF capture.