# RAMSES Arduino Gateway v0.4.0-beta.14 — GPIO UART loopback bench test

## New diagnostic
- Added **ESP32 GPIO26 → GPIO25** UART loopback, independent of CC1101 RF transmission. Bench mode uses UART1 TX GPIO26 and UART2 RX GPIO25 at 38,400 baud, 8N1. With your current wiring (CC1101 GDO0 disconnected from GPIO26, GPIO26 connected to GPIO25), one button press can test the actual electrical TX output.
- The firmware constructs the same fixed 29-byte `1FC9` offer used by the experimental pairing encoder (controller `01:160001`, bridge `10:048994`), encodes it into 70 UART bytes, sends it ONCE to GPIO26, listens on GPIO25, compares each byte, identifies the first mismatch and attempts to decode the captured RAMSES frame.
- Diagnostics UI includes explicit one-shot confirmation, live status and full expected/captured hex. An authenticated REST GET/POST endpoint `/api/v1/radio/tx-loopback` provides machine-readable verdicts, byte counts, mismatch index, 1FC9 decoding, UART timings and CC1101 MARCSTATE before/after.
- Requires all automatic TX, scan, polling, in-progress pairing and one-shot probes off. During test the CC1101 must remain in RX state; the function never invokes CC1101 transmit configuration or STX. No RF packet, bridge binding, or other actuator command is issued. No automatic repeats, NVS writes or reboot from the diagnostic.
- Added native tests for exact 70-byte fixture length, valid 1FC9 decode, matching bytes, corruption, truncation, extra bytes, missing signal and hex output bounds.

## Important distinction
A **pass** means a valid 70-byte Manchester-encoded UART sequence left ESP32 GPIO26 and reached ESP32 GPIO25 through the jumper, unchanged. It does **NOT** prove that the CC1101 GDO0 accepts that signal, that anything is transmitted over 868 MHz, or that the R8810A receives/accepts the command. After your electrical bench test, power off and restore **GPIO26 → CC1101 GDO0** to test RF separately later. Leave original Evohome boiler pairing undisturbed.

## Installation
Get `RAMSES-Arduino-Gateway-v0.4.0-beta.14-ESP32-WROOM-32E.zip` from the beta.14 release. See README for the physical GPIO loopback procedure.
