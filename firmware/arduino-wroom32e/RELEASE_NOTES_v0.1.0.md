# RAMSES Arduino Gateway v0.1.0 — ESP32-WROOM-32E

**Early hardware-testing prerelease.** Standalone, receive-only 868 MHz RAMSES II gateway for the ESP32-WROOM-32E (NodeMCU-32) and a compatible CC1101 transceiver.

## Download

**RAMSES-Arduino-Gateway-v0.1.0-ESP32-WROOM-32E.zip** contains an Arduino IDE sketch and all headers needed to compile it, a Wi-Fi/MQTT configuration template, project-specific setup instructions and the repository license. The adjacent .sha256 file verifies archive integrity.

This is deliberately a **separate release** from the Home Assistant Evohome Dashboard integration.

## Features

- Wi-Fi networking on ESP32-WROOM-32E (4 MB), with serial diagnostics on USB.
- CC1101 868.3 MHz RAMSES II receive-only pipeline, Manchester message decoder and checksum verification.
- Local read-only HTTP API: GET /api/v1/status and GET /api/v1/packets.
- MQTT RX reporting for possible downstream Home Assistant integration; no MQTT commands.
- Normal Evohome schedules and R8810A heating control remain independent.

## Open in Arduino IDE

1. Download and extract the ZIP.
2. Open RamsesGateway/RamsesGateway.ino.
3. Install the **esp32 by Espressif Systems** board package (Arduino core 3.x) and the **PubSubClient** library (2.8).
4. Copy GatewaySecrets.example.h to GatewaySecrets.h in the RamsesGateway sketch folder; configure the 2.4 GHz Wi-Fi network and optional MQTT broker.
5. Select **ESP32 Dev Module**, flash size **4 MB**, and your serial COM port.
6. Verify the CC1101 pin labels and 3.3 V wiring before powering on; then compile/upload and monitor at 115200 baud.

**Safety:** Only power the CC1101 at 3.3 V. This prototype does not expose RF transmit functionality.

## Verification status

- **PASS:** Arduino compiler for ESP32-WROOM-32E, 4 MB.
- **PASS:** deterministic RAMSES Manchester framing/checksum/packet-format tests.
- **NOT TESTED:** actual ESP32/CC1101 RF reception, Evohome decoding, long-run reliability, RF signal quality and end-to-end Home Assistant compatibility.

Please treat this as an experiment and report serial log + JSON status results before relying on the gateway.

## Attribution

Some register values and radio decoding concepts were adapted from [IndaloTech/ramses_esp](https://github.com/IndaloTech/ramses_esp), copyright Peter Price. Review original licensing before third-party redistribution.
