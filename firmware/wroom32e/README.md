# RAMSES-ESP port: ESP32-WROOM-32E (4 MB)

This directory contains a **prototype port overlay** for the open-source [IndaloTech RAMSES-ESP](https://github.com/IndaloTech/ramses_esp) firmware. This is **not** a validated firmware release. It preserves the upstream CC1101 radio receiver/decoder and Wi-Fi/MQTT gateway while adapting classic ESP32 hardware.

- Upstream pinned to commit: 12836b2907cb0f0a0b1f3be648dafc40192c2023.
- Target: ESP32-WROOM-32E, 4 MB flash, dual-core, ESP-IDF 5.2.1.
- Host interface: UART0 over USB-to-serial; local REST endpoint GET http://DEVICE-IP/api/v1/status.
- Radio: CC1101 at 868.3 MHz, **receive-only** initially. MQTT RX retains the upstream RAMSES-ESP format.
- The API reports rf_verified:false until RF has been confirmed on real hardware. A running server is not evidence of RF reception.

## Hardware connections

Check the **actual labels** on the CC1101 breakout before applying power. Pin ordering varies.

| CC1101 signal | NodeMCU-32 / ESP32-WROOM-32E |
|---|---|
| VCC | 3V3 **only** |
| GND | GND |
| SCK | GPIO18 / P18 |
| MOSI / SI | GPIO23 / P23 |
| MISO / SO | GPIO19 / P19 |
| CSN / CS | GPIO5 / P5 |
| GDO0 | GPIO26 / P26 |
| GDO2 | GPIO27 / P27 |

Both GDO lines are required by the full RAMSES decoder. SPI-only probing can use the first six wires. Never supply 5V to the CC1101. Use a **868 MHz** RF variant with suitable antenna. Verify physical pin spacing; 2.0 mm radio pins may not fit 2.54 mm Dupont connectors.

## Port scope

1. Targets ESP32 instead of ESP32-S3, with the radio task on core 1 and networking on core 0.
2. Replaces S3 USB Serial/JTAG command input with UART0; leaves UART1 available for CC1101 GDO2.
3. Changes GPIO assignments and disables debugging on GPIO6-11 (connected to flash on classic ESP32).
4. Uses 4 MB flash layout with two 1.875 MB application slots; **firmware fit awaits a successful build**.
5. Blocks serial and MQTT initiated RF transmission during receive-only bring-up.
6. Adds a read-only status REST endpoint. RAMSES RF packets are still forwarded via MQTT.
7. Prevents accidental fetching of S3 OTA releases with an invalid update URL. **OTA is not validated**.

## Reproducible build

Install ESP-IDF 5.2.1, or use GitHub Actions CI.

~~~bash
git clone https://github.com/IndaloTech/ramses_esp.git
git -C ramses_esp checkout 12836b2907cb0f0a0b1f3be648dafc40192c2023
python firmware/wroom32e/apply_port.py ramses_esp
cd ramses_esp
idf.py set-target esp32
idf.py menuconfig
idf.py build
~~~

The script is located in the **HA Evohome project**, not the upstream repository: adjust its path depending on where both folders were cloned.

In menuconfig, set the 2.4 GHz Wi-Fi SSID/password under Network Configuration; do not commit the generated sdkconfig, which may contain secrets. Use RAMSES-ESP network commands to configure MQTT broker. After a successful build and wiring verification, you can flash with ESP-IDF on Windows using: idf.py -p COM5 flash monitor.

## Evidence checklist

- [ ] Clean ESP-IDF build succeeds with the 4 MB partition table.
- [ ] Inspect CC1101 breakout pin labels and confirm correct frequency variant.
- [ ] Read CC1101 part/version over SPI.
- [ ] WROOM32E boots; UART and RF interfaces do not conflict.
- [ ] Wi-Fi joins LAN; REST endpoint responds.
- [ ] Valid RAMSES II frames observed over MQTT from Evohome/R8810A.
- [ ] No transmissions emitted (receive-only invariant).
- [ ] Sustained RF + Wi-Fi reliability confirmed.

**Not yet verified**: compiled image, application size, physical radio reception, RF timing under Wi-Fi load, or the exact AliExpress CC1101 breakout. The original upstream code and license remain the source of the decoder implementation.
