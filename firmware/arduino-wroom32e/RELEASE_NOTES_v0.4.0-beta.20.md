# RAMSES Arduino Gateway v0.4.0-beta.20 — prevent CC1101 silence reset and diagnose slow web

## Observed on M5Stack M146 868 MHz
The gateway reports `CC1101=ok` but virtually no GDO2 UART bytes and no RAMSES syncs. After two minutes the previous firmware runs an unsolicited SRES, which can fail and change the status to `CC1101=missing`. Separately the web interface is sluggish, especially around Wi-Fi and MQTT activity.

## Changes
- **No SRES solely due to quiet reception.** The two-minute guard now issues a bounded warning, not a new `radio.begin()` hardware reset. Startup recovery for actual failed chip initialization remains.
- CC1101 status-register SPI read `0xFF` is preserved as an **invalid reading**, rather than masking it into misleading `MARCSTATE=31`. The RX-health endpoint remains read-only.
- Service the synchronous HTTP server before MQTT reconnect/processing in each main-loop pass.
- Poll the MQTT socket at 100 ms intervals (rather than on every 2 ms pass); keep periodic MQTT state and discovery publishing.
- Enable TCP_NODELAY on HTML/JavaScript responses and report the maximum duration of each response.
- The authenticated `/api/v1/status` JSON now exposes `http_max_ms`, `http_last_ms`, `http_slow_calls`, `mqtt_connect_max_ms`, `mqtt_connect_slow_calls`, `mqtt_loop_max_ms`, `mqtt_loop_slow_calls`, `web_asset_last_ms`, `web_asset_max_ms`, and `radio_silence_warnings`.
- Serial HEARTBEAT includes HTTP/MQTT elapsed maxima for diagnosis even when HTTP is slow.
- Host-side regressions forbid automatic SRES triggered only by lack of RAMSES sync and verify the HTTP-before-MQTT order.

## Important limitations
The log so far **does not prove** the M146 radio ever entered stable RX or that GDO2 was carrying RAMSES data after its 0x00/0x14 SPI identification. This build prevents a damaging recovery loop and measures latency; physical reception and web speed still require device validation.

**Safety:** RF TX stays disabled by default, without automatic binding, STX, OpenTherm writes, or boiler changes.

Standalone Arduino IDE source/binary ZIP (after required CI/review/release):
`RAMSES-Arduino-Gateway-v0.4.0-beta.20-ESP32-WROOM-32E.zip`.
