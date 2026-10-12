# RAMSES Arduino Gateway v0.3.0 — Home Assistant MQTT Discovery

**Separate experimental Arduino IDE release** for ESP32-WROOM-32E (4 MB) + 868 MHz CC1101.

## New
- Retained **Home Assistant MQTT Discovery** for one automatically discoverable RAMSES II Gateway device.
- Eight diagnostic entities: CC1101 radio detected, valid packets, rejected frames, dropped frames, RF UART bytes, RF syncs, free memory and uptime.
- Retained diagnostic state updates every 15 seconds at `RAMSES/GATEWAY/18:NNNNNN/status`.
- Existing retained `online` / last-will `offline` topic unchanged, verified by a Home Assistant MQTT listener.
- Discovery configurations are published again whenever MQTT reconnects.
- RAMSES Gateway v0.2.0 local browser debug console and optional recovery Wi-Fi access point retained.

## Download and install
Download `RAMSES-Arduino-Gateway-v0.3.0-ESP32-WROOM-32E.zip`. Extract, open `RamsesGateway/RamsesGateway.ino` in Arduino IDE, copy `GatewaySecrets.example.h` to `GatewaySecrets.h`, preserve your Wi-Fi/MQTT credentials, compile and upload. Install ESP32 Arduino core v3.x and PubSubClient 2.8. Never commit `GatewaySecrets.h`.

Select ESP32 Dev Module / 4 MB / correct COM port, click **Upload** (not Start Debugging). Your web diagnostics remain at the same ESP32 IP.

Under Home Assistant MQTT integration, confirm **Discovery** is enabled, then look under MQTT → Devices for **RAMSES II Gateway**. You can verify retained configuration with topic `homeassistant/#`, and online/state messages with `RAMSES/GATEWAY/#`.

## Safety and verification
- No RF transmission or heating-control functions included.
- No CC1101 connected yet in the user's reported physical test; the radio sensor should indicate **off**.
- Software compilation, packet decoder, diagnostic log tests, JSON discovery validation and packaging are CI-gated.
- HA Discovery appearing in the user's instance, real CC1101 reception and packet reliability still need on-hardware verification.
- API uses HTTP, not HTTPS; keep it on your trusted LAN.

This release is independent of the Home Assistant Evohome Dashboard's release and does not change its installed integration.
