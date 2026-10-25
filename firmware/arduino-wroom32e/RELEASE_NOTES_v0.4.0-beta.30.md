# RAMSES Arduino Gateway v0.4.0-beta.30 — Virtual-device local unpair

Both existing virtual RF roles remain available:
- Virtual additional **01: controller** with guarded R8810A 1FC9
  binding, ID25 verification and read-only OpenTherm access.
- Virtual **12: wireless room thermostat** with Evohome zone sensor
  1FC9 binding, timestamped room-temperature input and 30C9 reports.

## Separate safe local Stop & Forget actions

The authenticated web interface now presents explicitly confirmed
actions for both virtual identities, backed by independent POST endpoints:
`/api/v1/opentherm/pairing/forget` and
`/api/v1/thermostat/forget`.

Both APIs verify the intended current virtual device ID, require a
strong web password plus confirmation token, queue the state change on
the dedicated radio task, clear the relevant pairing and sensor state,
and preserve RF cooldowns. No unbind or other RF frame is emitted.
A frame already in progress when the request is queued may finish first.

**These are LOCAL-ONLY stops, not over-the-air unbinding.** They do not
erase an Evohome zone's stored wireless sensor ID or alter the
R8810A bridge's binding to the original Evohome 01: controller.
A sensor must be removed/replaced through the actual Evohome zone
configuration. Clearing the original R8810A binding is neither offered
nor performed.

The original controller and thermostat implementations, coexistence
guardrails, RX state monitoring, packet capture and tested ID25 read
remain unchanged. Native regression tests cover stop during an active
offer, cleared verification state, disabled temperature reporting,
stale reply rejection and preserved cooldown.

Release publication requires successful self-hosted Linux tests,
Arduino compiles, independent Codex P1/P2 review and immutable
beta.30 ZIP/checksum asset publication. Over-the-air unbinding is
intentionally NOT claimed.
