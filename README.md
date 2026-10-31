# Evohome Dashboard for Home Assistant

## Automatic HACS update notifications (from v0.3.5)

This is a **HACS integration**, unlike the separately installed Hyundai
Battery Insight **Home Assistant add-on**. Both now require a version bump for
runtime changes. Evohome also publishes a stable GitHub Release with the
**same version** as `custom_components/evohome_dashboard/manifest.json`
**automatically after all required checks pass on `main`**.

HACS uses these releases for update notifications. Only the stable
`vX.Y.Z` tags belong to the HA integration; `ramses-arduino-v...`
releases are independent ESP32 firmware betas. Updates are *offered* through
HACS/Home Assistant, not silently installed; the user still selects Install
and restarts HA for Python backend changes.

**One-time migration caveat:** If this repository was previously installed
from a commit rather than a release, HACS may require one final **Redownload**
of the new stable `v0.3.5` release to start tracking semantic versions.
Future stable releases should then appear in the usual HACS update entity
after its next metadata check. See [HA release notes](docs/HA_CHANGELOG.md).


## Passive room heating-demand monitoring (v0.3.2)

Evohome Dashboard can now listen to the existing RAMSES ESP32 gateway's MQTT
`/rx` packets and expose **observed heat demand per RF zone**, bounded
request duration, controller `0008` indications and R8810A RF activity.
The feature is **read-only**, optional, and does not modify the gateway,
thermostats, OpenTherm or gas usage.

Configure the real Evohome controller ID, gateway RX topic, and explicitly
verified RF zone-to-room names in the integration options. Unknown/missing
packets are not interpreted as zero demand. Demand-weighted duration is
**not** room gas consumption. See [configuration, accuracy limitations and
examples](docs/HEATING_DEMAND.md).


A new, independent and user-friendly Evohome heating integration with a responsive touchscreen-style **Evohome** page in the Home Assistant sidebar.

**Current HA integration version:** v0.3.5 (stable HACS release on successful main-branch CI). The RF demand feature is **not yet validated against live TRV demand packets**.

This project uses the Apache-2.0 licensed [evohome-async](https://github.com/zxdavb/evohome-async) Python transport library as a dependency; it does not include or reuse `colegates/evohome-control` implementation code.

## What is included

| Feature | Phase 1 |
|---|---|
| All zones, current / target temperatures | Implemented |
| Native Home Assistant climate entities with heat/frost-protection | Implemented in v0.2.0 |
| System mode select (Auto, Eco, Away, Day Off, Heating Off when supported) | Implemented in v0.2.0 |
| System-wide Heating Off switch | Implemented if controller supports it |
| Individual zone frost-protection switches | Implemented in v0.2.0 |
| Native zone override preset modes and schedule resume buttons | Implemented in v0.2.0 |
| Cloud-exposed zone status, configuration, faults and temperatures as entities | Implemented in v0.2.0 |
| Permanent / time-limited temperature adjustments | Implemented |
| Resume native controller schedule | Implemented |
| System quick modes (offered by controller) | Implemented |
| Domestic hot water, if installed | Implemented |
| Seven-day graphical schedule editor | Implemented |
| Conflict prevention + schedule read-back | Implemented |
| Settings, device capability inventory | Implemented |
| Installer-only / hidden settings | **Informational only — cloud API access unverified** |
| Persistent, redacted error log | Implemented |
| HomeBoard integration / direct RF | Not in Phase 1 |

## Install

You need Home Assistant with HACS and a Honeywell/Resideo European Total Connect Comfort account.

1. In **HACS → Custom repositories**, enter `https://github.com/rvdheuvel72/HA---Evohome-Dashboard` and select **Integration**. This repository is the Python integration backend.
2. Download **Evohome Dashboard** and restart Home Assistant.
3. Open **Settings → Devices & services → Add integration → Evohome Dashboard**.
4. Sign in using your European Total Connect Comfort account credentials.
5. A new **Evohome** sidebar entry is registered for Home Assistant administrators.
6. Start by checking measured temperatures and system mode. Test one zone override and resume; only then test schedule edits.

**Avoid running the built-in `evohome` integration against the same Resideo account while testing.** Excessive independent logins can trigger Resideo rate limits. Account access requires the Resideo cloud and working Internet access.

### Configuration

The integration's **Configure** action provides temperature polling interval (default 180 seconds, minimum 60 seconds). Weekly schedules are fetched **on demand**, not on every polling cycle.

### Safety and observability

- Home Assistant stores OAuth access/refresh tokens in private `.storage`, separated by configuration entry. Never add these files to Git.
- Evohome Dashboard 0.3.1 uses `evohome-async==2.1.0`, matching Home Assistant 2026.10's built-in integration. Version 2.1.0 returns no `CommTask` from cloud writes: the dashboard submits one write, then polls Resideo for the requested state. Schedule saves also guard against concurrent edits and verify results with repeated live GETs. A cloud GET confirmation is not a direct RF acknowledgement.
- The Error log sidebar tab provides the last 80 diagnostic incidents, persisted across HA restarts. Only timestamp, operation, exception class and target identifier are saved.
- If HA or Resideo becomes unavailable, Evohome retains its existing locally stored schedule. The frontend displays the last known data and connection health.
- Installer-only settings (RF pairing, boiler tuning, Wi-Fi, factory reset and device diagnostics) are clearly displayed as **unavailable through verified TCC cloud capabilities**. They cannot honestly be made controllable without further reverse engineering and hardware validation.

### Hardware acceptance checklist

A passing CI build **does not** mean end-to-end integration is verified. Before relying on the system:

1. Check successful TCC sign-in and all expected zones are present.
2. Confirm measured / target temperatures match physical touchscreen.
3. Change one setpoint for one hour; wait for the physical Evohome display to update.
4. Cancel override and verify Follow Schedule.
5. Export or manually record the current weekly schedule before an edit.
6. Edit one day, save, and compare resulting schedule on the physical display.
7. Confirm quick actions and hot water only when your hardware supports them.
8. Disconnect Internet briefly and check honest offline/error states; do not assume local control.

## Login issues

Version 0.3.1 restores the built-in HA 2026.10 transport pin `evohome-async==2.1.0` after version 0.2.0 introduced 3.0.0 and could leave shared Python dependencies inconsistent (for example `No module named '_evohome.exceptions'`). It uses the 2.1.0 public client/location `update()` API instead of 3.0.0 `setup()` and `get_status()` methods. The integration also corrects status refresh calls and classifies invalid credentials, Resideo HTTP 429 rate limiting, invalid cloud responses and network errors separately. HA logs record only exception class and HTTP status, never raw vendor response bodies or secrets.

**After upgrading in HACS, restart Home Assistant and retry adding the integration.** If Resideo rate-limits the account, wait before retrying; repeated submissions can extend the lockout.

## Development / CI

CI deliberately uses **`[self-hosted, Linux, X64]`** runners. The workflow provisions Node 24, then executes Python compileall, pure schedule tests, JavaScript syntax checks and packaging-contract checks.

```bash
python3 -m compileall -q custom_components tests
python3 -m unittest discover -s tests -v
python3 scripts/validate_contract.py
node --check custom_components/evohome_dashboard/frontend/dashboard.js
node --check frontend-package/dist/evohome-dashboard-ui.js
python scripts/package_frontend.py
python scripts/verify_library_api.py  # in an isolated venv with evohome-async==2.1.0
```

See [Architecture](docs/ARCHITECTURE.md) and [Feature matrix](docs/FEATURE_MATRIX.md).

## Licensing

The original code in this repository is MIT-licensed (see `LICENSE`). The separate `evohome-async` dependency is Apache-2.0 licensed.

## Project boundaries

**Phase 1:** an improved Evohome touchscreen experience in Home Assistant only.

**Not now:** HomeBoard work schedules, household presence automation, RAMSES RF adapter and unsupported low-level installer controls.

## Codex quality gate

Before merging into main, all Codex P1/P2 findings must be fixed and resolved, and Codex must review the exact latest PR head. See [Merge policy](docs/MERGE_POLICY.md). A GitHub ruleset must be enabled to enforce the check; CI checks alone can be bypassed by privileged users.

### v0.1.2 (review fixes)

- Harden temporary-override expiry verification (zones, hot water, operating modes) against stale cloud values and minute truncation.
- Add Codex P1/P2 review gate, with the privileged check isolated on GitHub-hosted infrastructure; other validation remains on self-hosted runners.
- New merge policy requires completed exact-head Codex review and addressed P1/P2 findings before merge.

## v0.2.0: how to use the complete display

The default Home Assistant **climate** pop-up remains the native HA thermostat.
It does not contain the custom weekly schedule editor.

After restart, click **Evohome** on Home Assistant's left sidebar to open
the complete heating interface. It provides overview, individual rooms,
**Schedules**, quick operating modes, zone settings and error history.

You can also embed the full Evohome display into a Lovelace dashboard:

1. HA Settings → Dashboards → Resources → Add resource.
2. URL: `/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js` (once the separately packaged UI is installed).
3. Resource type: **JavaScript module**.
4. Add a manual card with:
   ```yaml
   type: custom:evohome-dashboard-card
   ```
5. Reload the browser and select **Schedules** in the card.

From the ordinary HA entity views, use the **Operating mode** select
on the Evohome controller device for quick actions and **Heating enabled**
to enable Auto / Heating Off (if offered by your controller).
Each zone offers native **OFF** (frost/minimum temperature) / **HEAT**
(follow schedule), a preset selector for temporary/permanent overrides,
a dedicated frost-protection switch, and a resume-schedule button.
**OFF per zone is not a true power-off: Evohome maintains frost protection.**

See [User guide](docs/USER_GUIDE.md) for the exact controls and limitations,
and [Renovate setup](docs/RENOVATE.md) for required app authorization.

Version 0.2.0 originally upgraded its Resideo client from 2.1.0 to **3.0.0** (subsequently reverted in backend v0.3.1 for HA 2026.10 compatibility),
including communication-task completion and state read-back.
A clean compile or contract test does not replace live controller acceptance.

## v0.3.0: fewer Home Assistant restarts

**Backend (this repo)** — Python integration, cloud API, sensors, secure HA
WebSocket commands and panel registration. Update via HACS **Integration**.
The update to backend v0.3.0 requires **one HA Core restart** so the new
frontend loader is registered. Python/backend updates still require HA Core
restart, not a host/system reboot.

**Frontend (separate HACS Dashboard repo)** — independent JavaScript sidebar,
room controls, schedule editor and Lovelace card. Its source and publishable
HACS package are in [`frontend-package/`](frontend-package/) until the
dedicated `rvdheuvel72/evohome-dashboard-ui` GitHub repository is created.
Once published and installed via HACS **Dashboard**, future UI-only updates
require **only a browser refresh**, not a HA Core restart. The HACS
`/hacsfiles/` resource serves updates without caching.

The backend loader at `/api/evohome_dashboard/dashboard.js?v=0.3.0` first
loads `/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js`. If absent,
it uses the **frozen v0.2 UI fallback** to avoid breaking old installations.
No installation of the separate HACS package is forced during migration.

### Publish the frontend independently

The frontend GitHub repository does not exist until created by its owner.
Create a public repository named **evohome-dashboard-ui** under
`rvdheuvel72` and publish the **contents** of `frontend-package/` to
its repository root. The built artifact in CI contains exactly these files:

```
hacs.json
README.md
LICENSE
dist/evohome-dashboard-ui.js
```

The package can be built using
`python3 scripts/package_frontend.py --output build/evohome-dashboard-ui.zip`.
HACS then installs it as a **Dashboard** repository.

See [Frontend migration](docs/FRONTEND_MIGRATION.md) for step-by-step upgrade
instructions and the limitations. **The separate repository must be
published before no-restart HACS UI updates become available.**
