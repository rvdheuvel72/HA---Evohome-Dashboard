# Evohome Dashboard for Home Assistant

A new, independent and user-friendly Evohome heating integration with a responsive touchscreen-style **Evohome** page in the Home Assistant sidebar.

**Current release:** Phase 1 / v0.1.1 — source and CI validation; **not yet validated on a live Evohome controller**.

This project uses the Apache-2.0 licensed [evohome-async](https://github.com/zxdavb/evohome-async) Python transport library as a dependency; it does not include or reuse `colegates/evohome-control` implementation code.

## What is included

| Feature | Phase 1 |
|---|---|
| All zones, current / target temperatures | Implemented |
| Native Home Assistant climate entities | Implemented |
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

1. In **HACS → Custom repositories**, enter `https://github.com/rvdheuvel72/HA---Evohome-Dashboard` and select **Integration**. As this repository is currently private, your HACS/GitHub account must have access to it.
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
- Pinned evohome-async 2.1.0 returns no communication-task handle. Control writes therefore poll Resideo for the requested state; schedule writes also guard against concurrent changes and verify the schedule by repeated live reads. A cloud GET confirmation is not the same as direct RF acknowledgement.
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

Version 0.1.1 fixes an API mismatch in v0.1.0: `EvohomeClient.setup()` did not exist in the pinned `evohome-async==2.1.0`. It also corrects status refresh calls and classifies invalid credentials, Resideo HTTP 429 rate limiting, invalid cloud responses and network errors separately. HA logs record only exception class and HTTP status, never raw vendor response bodies or secrets.

**After upgrading in HACS, restart Home Assistant and retry adding the integration.** If Resideo rate-limits the account, wait before retrying; repeated submissions can extend the lockout.

## Development / CI

CI deliberately uses **`[self-hosted, Linux, X64]`** runners. The workflow provisions Node 22, then executes Python compileall, pure schedule tests, JavaScript syntax checks and packaging-contract checks.

```bash
python3 -m compileall -q custom_components tests
python3 -m unittest discover -s tests -v
python3 scripts/validate_contract.py
node --check custom_components/evohome_dashboard/frontend/dashboard.js
python scripts/verify_library_api.py  # after installing evohome-async==2.1.0
```

See [Architecture](docs/ARCHITECTURE.md) and [Feature matrix](docs/FEATURE_MATRIX.md).

## Licensing

The original code in this repository is MIT-licensed (see `LICENSE`). The separate `evohome-async` dependency is Apache-2.0 licensed.

## Project boundaries

**Phase 1:** an improved Evohome touchscreen experience in Home Assistant only.

**Not now:** HomeBoard work schedules, household presence automation, RAMSES RF adapter and unsupported low-level installer controls.

## Codex quality gate

Before merging into main, all Codex P1/P2 findings must be fixed and resolved, and Codex must review the exact latest PR head. See [Merge policy](docs/MERGE_POLICY.md). A GitHub ruleset must be enabled to enforce the check; CI checks alone can be bypassed by privileged users.
