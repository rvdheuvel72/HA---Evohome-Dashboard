# ESP32 RAMSES cross-task ownership audit

Reviewed source: `RamsesGateway.ino`, `Cc1101Radio.h`,
`RamsesUartFifo.h`, `OpenThermGateway.h` on 30 October 2026.
This is a source-level audit, **not** evidence of an observed runtime race.
No device logs, sanitizer traces, or hardware packet-corruption measurements
were available for this audit.

## Verified ownership and controls

| Object / resource | Producer | Consumers | Current control | Assessment |
| --- | --- | --- | --- | --- |
| CC1101 SPI / `Cc1101Radio` | `radioTask` | `radioTask` | Single-task ownership documented in sketch; HTTP reads published reports | Do not add ISR spinlocks around SPI; no ISR ring in `Cc1101Radio.h` |
| RF UART byte stream / RAMSES decoder | `radioTask` | `radioTask` | Serial driver ring internally; decoding in radioTask; no custom ISR indices in `RamsesUartFifo.h` | Gemini's specific custom ISR ring claim not reproduced |
| `rxQueue`, `debugQueue` | radioTask | `loop()` | FreeRTOS queues | Cross-core handoff uses task-safe primitives |
| `packetCapture` | radioTask | HTTP | `packetCaptureMux` | Protected, though measure worst-case critical-section duration before increasing capture size |
| `rxDiagnostics`, `rxHealth`, `spiAudit`, `uartProbe` | radioTask | HTTP | Dedicated `portMUX_TYPE` locks | Snapshot consistency is intentional |
| `thermostat` session | radioTask and HTTP commands | radioTask and HTTP | `thermostatMux` for selected operations | Spot-check every access if state machine changes |
| `validFrames`, `rfUartBytes` and related counters | radioTask | HTTP / main loop | Aligned volatile 32-bit snapshots | One-word reads/writes are atomic on the target ESP32; `volatile` is not a general cross-core synchronization primitive |
| `radioTaskStackMinFreeBytes` | radioTask every 15 s | HTTP | Single aligned volatile 32-bit value | New observational metric, not a multi-field state |
| `configuration.current` | setup (active configuration) | radioTask, HTTP | Staged NVS writes do not mutate active state in-place; restart applies new settings | Preserve this invariant |

## Specific follow-up: OpenTherm gateway cross-task checks

`OpenThermGateway.h` **does** use `mutex_` for requests, binding
state transitions, schedule snapshots, and radio-task processing, including
`requestCvBatch()`, `requestPairedBridgeRead()`, and `tick()`.

However, some preliminary readiness checks are outside the critical section,
such as `requestScan()` evaluating `binding_.busy()` and
`pairedBridgeRead_.busy()` before entering the mutex, and
`pairedBridgeReadBlockReason()` reading `requested_` and `work_.active`
outside its later locked section. `noteDevice()` sets `bridgeSeen_` and
`sourceConflict_` without the same mutex; HTTP accesses these for
readiness decisions.

**Inference / potential TOCTOU risk, not a verified failure:** a readiness
decision based on several separately observed fields may not represent one
consistent instant. The later locked transition still blocks many unsafe
operations, but a complete proof of mutual exclusion has not been performed.
Do **not** replace the short locked state updates with one broad lock around
RF TX, SPI, network operations, or HTTP responses.

## Recommended targeted next investigation

1. Instrument attempts to start overlapping binding / batch / one-shot / scan
   operations; record the chosen state and reject reason, without logging keys
   or credentials.
2. Host-native concurrency tests should interleave a queued HTTP request
   and radioTask state transition; assert that only one RF command may be
   selected and transmitted and that the scheduled CV batch remains bounded.
3. If a race is reproduced, move **all** multi-field readiness predicates
   and their corresponding state mutation into the *same short critical
   section*, or transfer commands from HTTP to radioTask using a bounded
   FreeRTOS queue. Preserve existing RF cooldown and one-shot limits.
4. Repeat on real ESP32 under Wi-Fi/MQTT reconnect and active packet receive,
   recording dropped frames, stack watermark, heap minima and HTTP latency.

## Runtime performance evidence

`/api/v1/status` publishes the free heap, minimum free heap,
largest available heap block, RX task stack lifetime high-water mark (bytes)
and the existing HTTP and web-asset latency metrics. Sampling the RX stack
at most every 15 seconds avoids affecting radio processing.

Use `scripts/check_gateway_health.py` with measured, device-specific
thresholds. It reads live HTTP diagnostics but does not change RF configuration.
Without real hardware measurements, no safe numerical baseline can be claimed.

**Unknowns:** actual runtime race occurrence, RX task stack minimum during
the complete pairing/batch path, long-term heap fragmentation, sustained HTTP
latency and whether a future ESP32 core changes task scheduling behavior.
