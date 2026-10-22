# RAMSES Arduino Gateway v0.4.0-beta.18 — web responsiveness and warm-boot RX recovery

## What is fixed
- In v0.4.0-beta.17 the 41KB embedded HTML/JS page could take minutes to load incompletely, showing `Connecting...` while `/api/v1/status` still worked. Split HTML and JavaScript into independently served PROGMEM assets, each transmitted in bounded 1KB blocks with exact Content-Length and a cooperative yield.
- Load the JavaScript using `/gateway.js` instead of waiting for a single monolithic page response.
- Deduplicate concurrent browser polls, slow status polling to 5 seconds, and poll specialized diagnostic details only while their panels are expanded and browser visible. Keep manual diagnostics on demand.
- Batch up to 20 debug-log entries per response rather than making one ESP32 network write per entry. Reduce MQTT retry frequency and bound MQTT socket reads.
- M5Stack M146 reboot resilience: proper CC1101 CSn/SRES pulse timing, bounded chip-ID retries, verify MARCSTATE is actually RX after initialization, and retry startup initialization at 5-second intervals if unavailable. No need to power-cycle on the first failed SPI setup.
- Correct built-in wiring table: the M5Stack M146 868MHz module needs **M-BUS pin 28 at 5V** via onboard regulator; a bare CC1101 breakout uses **3.3V**, not 5V.
- Retain PR #47 RX-health endpoint, and add a web-delivery regression test including Node.js syntax validation, plus standard RAMSES Arduino CI.

## Safety
Radio starts in `receive_only`. No automatic RF TX, pairing, binding, heating writes or R8810A reset. The reset sequence uses only CC1101 SRES, SIDLE/SRX — never STX. A physical soft-reset vs. power-cycle test is still required to confirm restoration of valid RAMSES II reception.

## Verification on ESP32
After flashing, load `http://evohome-gateway.local/`, check the page updates promptly and that the separate `/gateway.js` resource loads. Compare `/api/v1/status` and `/api/v1/radio/rx-health`, then reboot once and verify RX health and valid frames without cutting power.

The source and compiled binaries are bundled in `RAMSES-Arduino-Gateway-v0.4.0-beta.18-ESP32-WROOM-32E.zip`; release publication requires CI and exact-head review gate.
