# evohome-async v3.1.0 — experimental compatibility branch

**Branch:** `compat/evohome-async-3.1.0`. Do **not** merge or deploy yet.

Production `main` stays pinned to `evohome-async==2.1.0`. Stock Home Assistant
2026.10 still pins 2.1.0 for its built-in Evohome integration, whereas
[Home Assistant development](https://github.com/home-assistant/core/blob/dev/homeassistant/components/evohome/manifest.json)
uses 3.1.0. **Do not install this branch on HA 2026.10 alongside built-in Evohome**:
they share the `evohomeasync2` Python module. Cross-version coexistence is
unverified and may break startup, token refresh or runtime calls.

## Implemented

- Config flow / initialization: `EvohomeClient.setup()` (configuration-only).
- Refresh and post-write validation: `Location.get_status()`.
- All state writes (zone, system mode, DHW) and schedule writes await returned
  `CommTask.wait()` within a bounded 60-second timeout. An additional fresh
  GET must still confirm the physical controller/schedule state.
- Keep serialized writes, strict expiry checking, schedules cache and
  secret-free error handling.
- Pin 3.1.0 **only on this branch**, update native API/HA import tests,
  and assert CommTask ordering and fail-closed error propagation.

## CI limitations and promotion requirements

Self-hosted tests install v3.1.0 in separate Python 3.14 virtual environments.
The HA 2026.10 import smoke check **is not a coexistence test** and
does not authorize deployment. CI uses no production credentials.

Before future promotion: verify a **released** HA version with built-in
Evohome pinned to 3.1.0, test both integrations together, exercise real
token refresh, daylight-saving status, zone/DHW/mode and schedule edits,
and validate failed/slow CommTasks and Resideo rate limits. Require
self-hosted CI and fresh P1/P2 review before merging.

API references: https://github.com/zxdavb/evohome-async/tree/v3.1.0
