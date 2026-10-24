# RAMSES Arduino Gateway v0.4.0-beta.21 — RX-state regression recovery

## What changed
- Compared against **beta.16**, the last user-confirmed valid M5Stack M146 RAMSES packet: `059 RQ --- 01:245914 10:048994 --:------ 3EF0 001 00` (`sync=1 valid=1`).
- Preserve CC1101 RX instead of issuing an unconditional SIDLE/SFRX/SRX sequence every 30 seconds. RX re-entry now requires **two independent 5-second readbacks** confirming an actionable non-RX state (IDLE or RXFIFO_OVERFLOW), unchanged asynchronous register configuration, and an idle packet decoder.
- On boot, retry SRX once if a readable MARCSTATE remains IDLE or overflow; do **not** SRES just because packets are missing, or treat `0xFF` (failed SPI read) as confirmed non-RX.
- Add authenticated, passive **`GET /api/v1/radio/uart-probe`** showing UART1 RX byte count, counts of `0x00` and `0xFF`, and the last 48 received bytes as hex. The endpoint neither reads CC1101 SPI directly nor transmits or pairs.
- Add host-native regressions for receive-reentry policy, invalid status reads, UART ring wraparound, and compile the full ESP32 firmware.

## How to interpret results
- If `rx-health.latest.marcstate=13` but `/uart-probe` stays nearly empty, inspect the CC1101 GDO2 signal path / RF board/antenna; SPI chip ID alone does not certify RF reception.
- If `marcstate=255`, CC1101 status read failed or was unavailable; this is **not** an RX-state confirmation.
- A positive `valid_frames` change and actual RAMSES packet are needed to demonstrate restored RF reception.

**Important:** This is a candidate software mitigation and diagnostic build; no live M5Stack M146 RF reception has been verified in CI. RF TX, pairing, and boiler control remain disabled by default.

Arduino IDE/source and firmware ZIP: `RAMSES-Arduino-Gateway-v0.4.0-beta.21-ESP32-WROOM-32E.zip`.
