# Using Evohome Dashboard 0.2.0

## Installation and upgrade

1. Update **Evohome Dashboard** in HACS.
2. Restart Home Assistant; do not keep both the built-in `evohome` integration
   and this custom integration actively polling the same Resideo account
   while diagnosing login / rate limits.
3. Check that the **Evohome** item appears on the HA left sidebar
   for an administrator.
4. Navigate to **Evohome → Overview** to see all cloud-reported zones.
5. For a regular Lovelace dashboard, register the JavaScript module resource
   `/api/evohome_dashboard/dashboard.js?v=0.2.0` and add a manual card:
   ```yaml
   type: custom:evohome-dashboard-card
   ```

## What the two UIs mean

- HA's default climate pop-up (thermostat dial) uses HA entities and shows
  standard Heat/Off and preset options. This is intentionally not the weekly
  schedule editor.
- The Evohome sidebar and custom Lovelace card show the **complete UI**:
  Rooms, Schedules, Quick Actions, Settings, and Error log.

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
