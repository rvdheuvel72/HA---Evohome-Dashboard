# RAMSES II Gateway v0.4.0-beta.3 — ESP32-WROOM-32E

**Status:** Software beta, not physically RF-verified. PR #25 was merged after green host-native/Arduino firmware CI and exact-head Codex review with no P1/P2 findings. This release packaging is separately validated.

## What's new since beta.2

- **Diagnostics** and **Settings** are separate web tabs. The Settings tab retains all network, security, timezone, MQTT, RF, and read-only OpenTherm settings.
- New interactive CC1101 module-marking selector: 315 / 433 / 868 / 915 MHz, persisted to ESP32 NVS without recompiling after flashing.
- A receiver frequency field and explicit **experimental cross-band RX-only** option. This permits programming the CC1101 synthesizer to 868300 kHz while accurately identifying the pictured 433M V2.0 PCB as 433 MHz hardware.
- **Radio transmission is blocked** whenever cross-band RX is selected or the declared module band is not 868 MHz. All RF TX remains off by default.
- A wiring table for the photographed CC1101 V2.0 connector: pin 1 GND (brown) to ESP32 GND; pin 2 VCC (red) to ESP32 **3V3 only**. Remaining pins: GDO0→GPIO26, CSN→GPIO5, SCK→GPIO18, MOSI→GPIO23, MISO→GPIO19, GDO2→GPIO27.
- Legacy NVS v1 settings migrate to v2 preserving saved network and authentication settings.
- Extended frequency, cross-band RX, NVS migration and web UI tests.

## Important 433M hardware limitation

The CC1101 silicon supports 868 MHz synthesizer programming. **That does not validate the RF matching components or included antenna on a 433M-marked board.** The 433M V2.0 module vendor describes its PCB as supporting 387–464 MHz. Attempting 868.3 MHz with it is an **unsupported RX-only experiment**; successful register writes or SPI detection do not establish RAMSES reception. To confirm, inspect valid received Evohome packets. Do not enable OpenTherm RF read scans with the 433M board.

## Install / first test

1. Download `RAMSES-Arduino-Gateway-v0.4.0-beta.3-ESP32-WROOM-32E.zip` and its matching SHA256 from this release's Assets.
2. Disconnect ESP32 power, wire CC1101 as documented, and only reconnect USB after checking 3.3V/GND.
3. Unzip the Arduino sketch, copy `GatewaySecrets.example.h` to private `GatewaySecrets.h`, configure Wi-Fi/password and flash with Arduino IDE. Keep secrets out of Git.
4. Open the ESP32 web interface → **Settings** tab. Select **433 MHz PCB**, receiver **868300 kHz**, tick **experimental cross-band RX**, leave RF transmission **OFF** and save/restart.
5. Open **Diagnostics**. Verify CC1101 chip detection, UART bytes, synchronization count and valid RAMSES frames. Do not treat chip detection alone as successful RF reception.

**No OpenTherm boiler write commands are implemented.** Read-only RF requests exist in the project but remain guarded and are not hardware-certified. Keep the gateway on a trusted local network because its web HTTP Basic authentication is not encrypted.

## Known limitations

868 MHz reception with a 433M circuit and antenna; real CC1101/R8810A/Intergas reception, OpenTherm request transmit collision behavior, reliable RF response correlation and end-to-end Evohome/Home Assistant telemetry require physical verification. The Home Assistant integration itself is separate.
