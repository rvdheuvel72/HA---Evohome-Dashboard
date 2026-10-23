# RAMSES Arduino Gateway v0.4.0-beta.19 — CC1101 initialized but reported missing

## Symptom and root cause
On M5Stack M146 868 MHz with ESP32-WROOM-32E, firmware v0.4.0-beta.18 logged:
`CC1101 detected=0 PARTNUM=0x00 VERSION=0x14`
and repeated `CC1101 not initialized; retry in 5s` indefinitely. The chip was responding to SPI correctly. The new beta.18 startup requirement incorrectly treated a MARCSTATE reading that did not reach RX (0x0D) within 15 ms as **failure to detect the chip**; warm/cold boots can transiently show IDLE/settling states.

## Changes
- Separate CC1101 chip identification and success of the receive strobe from **advisory** startup MARCSTATE confirmation. No longer reset a responsive chip indefinitely just because the first RX state observation was transient.
- Continue to report `radio_detected` after a valid chip identity, frequency/register programming and accepted SRX command. `radio_verified` still requires a decoded RAMSES II message.
- New `cc1101_init_stage`, `cc1101_boot_marcstate`, and `cc1101_rx_at_boot` in the authenticated status API. Existing `/api/v1/radio/rx-health` remains the primary live RX-state diagnostic.
- Keep beta.18's responsive web UI and bounded RX-only recovery logic.
- Host-native C++ test exercises chip-ID acceptance and RX-state independence; HTTP regression and ESP32 compile remain enforced.

## Safety and limitations
Defaults remain `receive_only`; no unattended STX/transmission, binding or heating writes. This repairs a **false-negative radio initialization**. It does not prove ongoing GDO2/UART data reception, RAMSES packets or RF transmission. Compare `/api/v1/status` with `/api/v1/radio/rx-health` on the actual M146 after installation and software reset.

Arduino IDE source and ESP32 binaries: `RAMSES-Arduino-Gateway-v0.4.0-beta.19-ESP32-WROOM-32E.zip`, with SHA-256 checksum, published after CI/review/merge.
