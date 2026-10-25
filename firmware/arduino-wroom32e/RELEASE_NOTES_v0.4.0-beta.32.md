# RAMSES Arduino Gateway v0.4.0-beta.32 — prompt updates to virtual thermostat temperature

## Physical validation

On 2026-10-25, the actual Evohome test zone successfully displayed
the 18.50°C and subsequent 19.00°C values provided manually through
the ESP32 virtual `12:160002` thermostat under firmware beta.31.
Our last observed controller acceptance was zone index `03`.

## Fix

Previously, even a *changed* external temperature was held until the next
five-minute `30C9` transmission. The web UI said "accepted" without
distinguishing the saved temperature from the transmitted value.

- A fresh changed measurement now becomes eligible for RF TX **15 seconds**
  after the prior attempt. The very first valid reading can transmit
  immediately after pairing.
- Repeated updates within the 15-second guard interval coalesce into the
  latest reading; identical updates do not generate extra RF.
- Unchanged valid samples retain their five-minute refresh interval, and
  all readings expire after the 15-minute stale-data deadline.
- Failed local TX attempts do not spin; retries respect the 15-second
  guard. Concurrent HTTP/MQTT updates during RF TX are not lost or
  falsely marked sent.
- The REST status and UI show saved reading, whether a new value is
  pending, time to next eligible attempt, last transmitted sample and
  local TX outcome. The event log includes the actual transmitted °C.
- Added native tests for change detection, retry throttle, in-flight
  snapshot races, unchanged temperature, expiry, and Web UI status fields.

The virtual `01:` R8810A controller role, virtual `12:` room
thermostat role, successful beta.31 DTS92 `1FC9` binding and `30C9`
wire encodings, authenticated local-only stop/forget, security and
sensor freshness protections are preserved.

**Physical Evohome reception of beta.32's faster updates is not yet tested.**
The local RF TX diagnostic is *not* an over-the-air delivery receipt.
