# RAMSES Arduino Gateway v0.2.0 — Web Debug Edition

**Standalone receive-only prerelease for ESP32-WROOM-32E + 868 MHz CC1101.** This is a separate Arduino IDE release from the Evohome Home Assistant integration.

## New in v0.2.0

- Browser-based diagnostics at **http://ESP32-IP/**, updating every 2 seconds.
- SPI/CC1101 identification (PARTNUM / VERSION), RF UART bytes, RAMSES synchronization counts, valid/rejected/dropped frames, RSSI, uptime, free heap, Wi-Fi and MQTT connection states.
- 64-entry bounded, severity-filterable diagnostic ring, with JSON log API and downloadable text export.
- Repeating **15-second heartbeat** in Serial Monitor to help diagnose an otherwise silent board.
- Optional WPA2 recovery Wi-Fi AP (configure your own password in GatewaySecrets.h); connect to RAMSES-Debug-XXXXXX and browse to **http://192.168.4.1/** even when your home Wi-Fi is unavailable.
- Optional HTTP Basic authentication (configure unique credentials; HTTP is not TLS).

## Download

**RAMSES-Arduino-Gateway-v0.2.0-ESP32-WROOM-32E.zip** includes the complete Arduino sketch folder (.ino and all four headers, including GatewayDebug.h and GatewayWebPage.h), an example configuration file, and setup instructions. A .sha256 asset provides an integrity checksum.

## Start

1. Extract ZIP and open **RamsesGateway/RamsesGateway.ino** in Arduino IDE 2.x.
2. Install Espressif Arduino core 3.x and **PubSubClient 2.8** library.
3. Copy **GatewaySecrets.example.h** to **GatewaySecrets.h** and configure 2.4 GHz SSID / password. Set your own **GW_RECOVERY_AP_PASSWORD** to enable the independent diagnostics AP.
4. Select **ESP32 Dev Module** / 4 MB / COM port, then use **Upload**, not Start Debugging (the NodeMCU-32 does not have the external JTAG debugger used by Arduino IDE's OpenOCD configuration).
5. In Serial Monitor select 115200 baud, press EN with the monitor already open, or connect to your configured recovery AP to inspect status.

**Red power LED staying lit is normal**, and does not indicate success or failure of the sketch.

## API (read-only)

- GET / — dashboard
- GET /api/v1/status — diagnostic state/counters
- GET /api/v1/packets — recent valid packets
- GET /api/v1/debug/logs?since=0&level=3 — diagnostic log polling
- GET /api/v1/debug/download — text log export

## Security and verification

- This is receive-only. No CC1101 RF transmission commands, no heating-control endpoints.
- HTTP is unencrypted: **LAN/recovery AP only**; never port-forward it.
- Wi-Fi/MQTT/debug credentials are not included in release archives or logs.
- **Verified in CI:** Arduino compile, RAMSES decoder test vectors, debug ring tests and ZIP integrity.
- **Not verified:** CC1101 on the user's physical ESP32, real RAMSES II reception, long-duration Wi-Fi/RF reliability or the cause of any silent Serial Monitor.
- Boot diagnostics and RF counters help localize the problem but do not prove reception without actual RF observations.

RF parameters/Manchester decoding ideas are based on IndaloTech/ramses_esp by Peter Price; observe upstream licensing requirements when redistributing derivatives.
