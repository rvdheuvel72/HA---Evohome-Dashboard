# Arduino-framework RAMSES II gateway for ESP32-WROOM-32E

Standalone, receive-only Arduino IDE prototype for the 4 MB ESP32-WROOM-32E / NodeMCU-32, CC1101 868 MHz, Wi-Fi REST API and MQTT RX. **Not yet tested on physical hardware.**

## Wiring (verify actual labels on CC1101 breakout)

| CC1101 | NodeMCU-32 |
|---|---|
| VCC | 3V3 ONLY |
| GND | GND |
| SCK | GPIO18 / P18 |
| MISO / SO | GPIO19 / P19 |
| MOSI / SI | GPIO23 / P23 |
| CS / CSN | GPIO5 / P5 |
| GDO0 | GPIO26 / P26 |
| GDO2 | GPIO27 / P27 |

**Disconnect USB before wiring; never connect the CC1101 to 5V.** Some breakout boards use 2.0 mm pitch, so verify jumper compatibility rather than forcing 2.54 mm Dupont sockets. Radio and antenna must support 868 MHz.

## Compile and upload from Arduino IDE

1. Install Arduino IDE 2.x and the Espressif board support URL: https://espressif.github.io/arduino-esp32/package_esp32_index.json
2. Install esp32 by Espressif Systems (Arduino core 3.x) in Boards Manager.
3. Install PubSubClient by Nick O'Leary (v2.8) in Library Manager.
4. Open RamsesGateway/RamsesGateway.ino. Select ESP32 Dev Module, 4MB flash, 240MHz, and COM5 (if unchanged).
5. Copy GatewaySecrets.example.h to GatewaySecrets.h in the sketch folder. Fill in Wi-Fi credentials and, optionally, an MQTT broker. The secrets file is gitignored. Without it, the sketch compiles but networking is disabled.
6. Click Verify, then Upload, and open Serial Monitor at 115200 baud.
7. Confirm CC1101 detected = 1, Wi-Fi IP, API status, then monitor valid_frames with your Evohome nearby. Do not change Evohome settings for this test.

## API

- GET / returns a minimal device page.
- GET /api/v1/status returns connection/radio/counter/health information.
- GET /api/v1/packets returns up to 12 recent valid RAMSES II messages.

There are NO write, pairing, transmit or heating-control endpoints. API is unauthenticated: trusted LAN use only; never port-forward it.

## MQTT

RX topic: RAMSES/GATEWAY/18:NNNNNN/rx; payload is JSON with msg (HGI80-style) and ts (UTC ISO 8601), similar to IndaloTech RAMSES-ESP. Retained online/offline state: RAMSES/GATEWAY/18:NNNNNN. The ID is derived from ESP32 hardware ID; no MQTT TX subscriptions or commands are present.

Packets are not published to MQTT until network time has synchronized (NTP). Serial and REST remain available.

## Implementation and validation

CC1101 configuration follows the 868.3 MHz / 38.38 kbps radio parameters in IndaloTech/ramses_esp. CC1101 GDO2 connects to ESP32 UART1 at 38400 baud. The framing decoder handles RAMSES sync, Manchester nibble pairs and modulo-256 message checksums. RX is pinned to core 1; Arduino setup/loop may also be pinned to core 1 depending on build configuration, so scheduler/RF timing is NOT yet validated.

RSSI value is an approximation. Status exposes radio_verified=false until real radio reception is confirmed. ESP32 core compilation alone does not establish RF correctness.

This project is independent of the existing HA Evohome integration; the heating controller retains full local scheduling.

## Licensing and provenance

Receiver radio values and frame/Manc​hester decoding rules were adapted from Peter Price's IndaloTech/ramses_esp: https://github.com/IndaloTech/ramses_esp . Review upstream licensing and secure permission where required before redistributing derived firmware.
