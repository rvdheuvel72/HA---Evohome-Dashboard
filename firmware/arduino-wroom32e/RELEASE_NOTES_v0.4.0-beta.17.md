# RAMSES Arduino Gateway v0.4.0-beta.17 — M5Stack M146 RX-health diagnostics

## What changed
- Includes PR #47 read-only CC1101 RX health instrumentation for the ESP32-WROOM-32E and M5Stack M146 868 MHz module.
- Authenticated `GET /api/v1/radio/rx-health` reports latest CC1101 MARCSTATE, IOCFG2, PKTCTRL0, static GDO2/GPIO27 level, UART counter and 30-second RX re-entry before/after snapshots.
- An additional diagnostics panel shows RX-health JSON, sampled approximately every 5 seconds while decoder is idle; 5ms status settling window on the existing RX recalibration.
- Updated documentation differentiates M5Stack M146 5V M-BUS power from bare CC1101 3.3V breakouts.
- Host-native RX health tests and WROOM-32E firmware compile on self-hosted Linux CI; isolated Arduino toolchain configuration prevents persistent runner state collisions.

## Hardware validation still needed
One valid `3EF0 RQ` RAMSES II packet was decoded using the M5Stack M146, but UART data stalled afterward on the user's ESP32. These diagnostics isolate whether the CC1101 stays in RX; they do not themselves repair RF reception or prove TX.

**Safety:** RF transmission remains disabled by default. No automatic pairing, RF TX attempts, R8810A reset, or heating write is introduced. Leave existing native Evohome controller/bridge binding untouched.

## How to test
Flash firmware from the ZIP using the existing local `GatewaySecrets.h`, then open `http://evohome-gateway.local/api/v1/radio/rx-health`. Compare `latest.in_rx`, `before_recalibration`, `after_recalibration`, `samples_not_in_rx`, `recalibrations_not_in_rx` and `rf_uart_bytes`. Keep mode `receive_only` until reception is consistently verified.

Versioned binary ZIP is published only after source tests, exact-head CI and review checks, and release readiness gates.
