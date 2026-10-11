# Migration: no-restart Evohome Dashboard UI releases

## Target architecture

| HACS category | Repository | Function | Updating |
| --- | --- | --- | --- |
| Integration | `rvdheuvel72/HA---Evohome-Dashboard` | Python coordinator, 6 entity platforms, Resideo TCC API, admin WebSockets, sidebar registration, fallback loader | Home Assistant **Core restart** for Python/dependency upgrades |
| Dashboard | `rvdheuvel72/evohome-dashboard-ui` | JavaScript interface, visual schedules, zone controls, Lovelace card | **Browser refresh only** |

HACS Dashboard assets are installed under `<config>/www/community/evohome-dashboard-ui/`
and served via `/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js`,
which HACS explicitly serves without browser caching.

## Existing installations

**Step 1 — Upgrade the Python backend to v0.3.0 (one-time restart).**
In HACS update Evohome Dashboard and restart **Home Assistant Core**, not
the computer or Docker host. The existing sidebar remains named Evohome.

The backend statically serves a tiny `/api/evohome_dashboard/dashboard.js`
ES module loader (no cache headers), plus
`/api/evohome_dashboard/dashboard-legacy.js` for backward compatibility.
Until Step 3 succeeds, users keep seeing the v0.2 interface.

**Step 2 — Publish the independent frontend repository (repository owner).**

The GitHub connector currently has repository read/write permissions but **no
create-repository action**. The owner must create a new, empty *public* repo:
`https://github.com/rvdheuvel72/evohome-dashboard-ui`.
Do not initialize with a conflicting README unless intending to overwrite it.

Copy the **contents** (not the enclosing directory) of the backend repo's
`frontend-package/` into the new repository root, preserving:

```
evohome-dashboard-ui/
├── hacs.json
├── README.md
├── LICENSE
└── dist/
    └── evohome-dashboard-ui.js
```

The backend CI uploads an artifact named `evohome-dashboard-ui-hacs`
containing exactly these four files; or run locally:

```bash
python3 scripts/package_frontend.py --output build/evohome-dashboard-ui.zip
```

Extract and commit its contents to the **frontend repository root**.
The HACS frontend `filename` is `evohome-dashboard-ui.js`.

**Step 3 — Install the frontend once.** In HACS Custom repositories add
`https://github.com/rvdheuvel72/evohome-dashboard-ui` as **Dashboard**,
download it and refresh the HA browser with **Ctrl+Shift+R**.
No Core restart. The Evohome sidebar will import the newly served module.
If unavailable, it silently falls back to the legacy module, with a
non-sensitive console warning.

**Step 4 — Lovelace card users.** In HA Settings → Dashboards → Resources,
replace the old `/api/evohome_dashboard/dashboard.js?v=0.2.0` JavaScript
module resource (if any) with:

```
/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js
```

Use the unchanged card YAML:

```yaml
type: custom:evohome-dashboard-card
```

The sidebar works without manual Lovelace resource configuration.

## Subsequent updates

- **UI-only fix:** push `dist/evohome-dashboard-ui.js` to the standalone
  frontend repo, HACS update Dashboard element, hard-refresh browser.
  Home Assistant Core, heating control and entities remain running.
- **Backend fix:** update HACS Python integration and restart Home Assistant
  Core. It cannot hot-reload Python modules and installed dependencies safely.
- **Frontend removed / unavailable:** the backend loader falls back to frozen
  v0.2 UI; temperature and schedules continue to be controlled by the same
  Python integration.
- The old bundled fallback is intentionally frozen. New UI features belong
  in the standalone frontend repo only after publication.

## Acceptance checks

1. Open the existing Evohome sidebar with **only** the backend v0.3 installed.
   It must show the legacy UI without JavaScript errors.
2. Install the independent frontend package; browser hard-refresh.
   The sidebar and Lovelace card must render the new module.
3. Test all four zones and one non-destructive schedule read.
4. Change a UI label in the **frontend repository only**, update via HACS,
   hard-refresh browser. Check the label changed without HA Core restart
   or any climate entity becoming unavailable.
5. Temporarily remove the separate frontend file (test installation only),
   refresh and verify legacy fallback.
6. Verify a backend update still requires HA Core restart and that the
   Resideo data retains its secure WebSocket privileges.

This is a packaging/migration plan verified by source/unit tests. Physical
Evohome and actual HACS installs require in-home acceptance tests.
