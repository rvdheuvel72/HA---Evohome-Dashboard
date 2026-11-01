# Using Evohome Dashboard (source integration v0.3.7)

## Installation and upgrade

1. Update **Evohome Dashboard** in HACS to the latest published stable integration version (current source manifest: `0.3.7`).
2. Restart Home Assistant Core **once** for this Python backend upgrade; do not keep both the built-in `evohome` integration
   and this custom integration actively polling the same Resideo account
   while diagnosing login / rate limits.
3. Check that the **Evohome** item appears on the HA left sidebar
   for an administrator.
4. Navigate to **Evohome → Overview** to see all cloud-reported zones.
5. After a separate `evohome-dashboard-ui` repository is published, install
   the `Evohome Dashboard UI` HACS **Dashboard** package. Reload the browser,
   not Home Assistant. For a regular Lovelace dashboard, register the module
   `/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js` and add a manual card:
   ```yaml
   type: custom:evohome-dashboard-card
   ```

## What the two UIs mean

- HA's default climate pop-up (thermostat dial) uses HA entities and shows
  standard Heat/Off and preset options. This is intentionally not the weekly
  schedule editor.
- The Evohome sidebar and custom Lovelace card show the **complete UI**:
  Overview, Rooms, Heating demand, Schedules, Quick actions, Settings, Gateway test and Error log. The optional independent HACS Lovelace card may lack the newest sidebar tabs.

## Zone controls

- **Adjust**: choose a target temperature and 1h/2h/4h/24h or permanent override.
- **Resume schedule**: cancel the individual room override.
- **OFF / Frost protection**: request the zone's minimum setpoint as a
  permanent override, often 5°C. This does **not** switch off the RF valve,
  binding or heating supply. The frost threshold comes from zone capabilities.
- **Native HA preset**: Follow schedule, Temporary override (1h),
  Permanent override, and Frost protection.
- **Schedule**: open the actual weekly schedule for that zone, edit
  switchpoints, copy day and save; cloud GET conflict detection and
  write-task confirmation follow. Back up the original schedule before changes.

## Whole system / quick modes

Use the **Operating mode** select on the main Evohome controller device.
The available choices are **exactly** those offered by Resideo. If
`heating_off` is available, a separate Heating enabled switch is exposed.
Switching it back on requests **Auto**; it does not recover the previous
Away/Eco/Custom mode.

## Cloud values exposed in HA

Each zone has a climate entity, current and target temperature sensors, an
override mode sensor, override-until timestamp, a configuration/capability
sensor, a problem binary sensor, a frost-protection switch and resume button.
The system has operating-mode select, mode sensor, fault sensor and optional
Heating enabled switch. Hot-water capabilities are created only if supported.

Only information available from the Resideo cloud is exposed. The physical
Evohome controller's hidden/installer menus, RF pairing and boiler installer
parameters cannot be edited over the verified TCC API. Settings lists these
functions as unsupported instead of inventing controls.

## Gateway test: manual ESP32 OpenTherm ID25 diagnostic

The administrator-only **Evohome → Gateway test** tab is part of the
bundled Home Assistant sidebar panel (v0.3.7 source). It uses the existing
ESP32 gateway diagnostic endpoint, not the Resideo cloud.

1. Enter the ESP32's **private LAN IPv4 address** (not a URL).
2. Enter the gateway's **HTTP Basic Auth username and password**.
   Triggering requires a strong gateway password of at least 12 characters.
3. Select **Read status** to retrieve the current one-shot status.
4. Choose **UART** or **Reference FIFO**, then click **Run ID25 test**
   and acknowledge the explicit RF transmission confirmation.
5. Choose **Read status** again and inspect the JSON result and gateway logs.

Home Assistant must reach ESP32 port 80 over the LAN. Inputs remain in
the browser panel session through periodic refreshes, but are not stored
in HA configuration. The proxy accepts only this diagnostic endpoint.
A queued request or local transmission completion is **not** a verified
R8810A Read-Ack. No automatic pairing, OpenTherm write, or ESP32 firmware
update is included. Live hardware validation is still necessary.

## Diagnostics

- `Evohome → Error log` displays bounded redacted error history.
- Normal HA system logs show operations and exception *class*, but not
  response text, credentials, tokens or request bodies.
- Communication-task completion confirms Resideo gateway execution; it is
  not a substitute for comparing physical temperatures and schedules.

## Hardware verification checklist

Compare all zone values to the actual controller; test one temporary override
and automatic expiration; apply frost protection only when safe; confirm return
to schedule; test a single schedule edit and physical display readback;
confirm whole-system Heating Off only if supported.

**Never rely exclusively on untested software to protect pipes from freezing.**

## Backend vs frontend upgrades

- Python/backend upgrades (including source v0.3.7): one Home Assistant Core restart required.
- After publishing/installation of the separate HACS Dashboard frontend:
  UI/layout/schedule-editor updates → update frontend in HACS → hard refresh
  browser (Ctrl+Shift+R); Home Assistant Core stays running.
- The sidebar uses the maintained bundled JavaScript if the external package is missing or lacks required feature markers. Any current `/api/evohome_dashboard/dashboard.js`
  Lovelace resource remains valid as a compatibility loader.
- Install the standalone frontend as a HACS **Dashboard**, not as a second
  Python integration. No heating-device re-pairing is needed.

For repository publication instructions see [Frontend migration](FRONTEND_MIGRATION.md).
