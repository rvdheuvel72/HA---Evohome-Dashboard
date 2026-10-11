# Architecture: Home Assistant Evohome Dashboard

## Scope

Phase 1 is an original HACS integration and an administrator-only custom sidebar panel.
It does not use or embed any source from `colegates/evohome-control`.
Its transport uses the independently maintained, Apache-2.0 licensed `evohome-async==3.0.0` library (also used by the built-in HA Evohome integration). Resideo TCC v2 is cloud-only; local LAN control or RAMSES RF is **not** part of Phase 1.

### Components

- `config_flow.py`: login through the normal Home Assistant UI and adjustable poll interval.
- `auth.py`: per-account private token cache using Home Assistant `Store`; never exposed to the frontend.
- `hub.py`: HA `DataUpdateCoordinator` for status polling, verified status polling, schedule cache, optimistic edit conflict detection, cloud read-back verification, and error recorder.
- `model.py`: minimal, safe, browser-facing state. Never forward vendor raw bodies or auth tokens.
- `schedules.py`: pure constraints, normalized seven-day schedule validation and deterministic revision fingerprint.
- `websocket.py`: HA-authenticated, administrator-only commands. All writes validated again server-side.
- `climate.py` and `sensor.py`: native HA entities.
- `frontend/dashboard.js`: stable lightweight dynamic loader for the
  independently distributed HACS frontend package, with browser-refresh
  updates and fallback to frozen `dashboard-legacy.js`.
- `frontend-package/dist/evohome-dashboard-ui.js`: independently installable
  HACS Dashboard module, containing no Python/backend or account secrets.
  Published in its own GitHub repository; HACS serves it via cache-free
  `/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js`.
- `scripts/package_frontend.py`: reproducible frontend-only HACS archive for
  release to the second repository.

### Main UI journeys

1. **Overview / Rooms** — zone temperatures and targets; permanent or time-limited override; clear and resume.
2. **Quick Actions** — available system modes, resume via Auto; DHW on/off for 1h or resume.
3. **Schedules** — request the live weekly schedule per room, edit switchpoints, copy days, save with conflict check; the v3 transport waits for a cloud communication task, then verifies read-back state.
4. **Settings** — cloud connectivity and supported capability inventory. Installer-only features are read-only informational entries, not fake working controls.
5. **Error log** — bounded history in HA private storage; records operation, target, time, exception type only. HA log records the same without secrets.

### Failure behavior and critical boundaries

- The native Evohome controller keeps its last known schedule during HA/cloud outages.
- The UI displays last synced and cloud health; cloud commands report failure rather than success without confirmation.
- Schedule saves serialize writes per account, fetch a fresh copy, reject a stale fingerprint, write, and poll GET for matching state.
- A rejected or unverified write never silently claims success; no automated retry of non-idempotent writes.
- Polling intentionally excludes complete schedules; schedules load on request and are cached for 15 minutes.
- Authentication errors are categorized without logging vendor messages. Configuration-flow and runtime share a private, hashed account token-cache key to avoid duplicate password logins.
- Explicit user actions require administrator access; no independent exposed HomeBoard endpoint.
- Secrets are never included in errors or diagnostics.
- The physical controller's installer menus (RF pairing, boiler tuning, Wi-Fi/reset) have no confirmed control endpoint in the TCC API. No hidden-system modification is attempted. Future discovery must be capability-gated and opt-in.

### Deferred

HomeBoard, work-shift automations, presence learning, local RAMSES RF, direct LAN protocol, advanced boiler controls, additional localization, historical analytics.

## Validation limits

Tests and CI validate source syntax, package layout and pure schedule operations. A real HA instance, cloud account, and live Evohome controller are required to validate runtime setup and physical temperature changes. Neither should be inferred from passing static/unit tests.

### Operational restart boundary

Frontend-only HACS Dashboard updates never reload the Python integration.
The admin sidebar uses a stable JavaScript loader and a browser reload picks
up UI releases; a missing frontend package loads the legacy bundled module.
Backend Python/module/dependency changes still require HA Core restart.
Changing frontend code in the backend bundle no longer ships UI upgrades.
