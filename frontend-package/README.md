# Evohome Dashboard UI (HACS Dashboard)

Standalone frontend for the [Evohome Dashboard Python integration](https://github.com/rvdheuvel72/HA---Evohome-Dashboard).

## Installation

Prerequisite: Evohome Dashboard **backend v0.3.0 or later** is installed
from HACS → Integration. Updating the Python backend requires one HA Core restart.

1. Install **Evohome Dashboard UI** from HACS → Custom repositories →
   `https://github.com/rvdheuvel72/evohome-dashboard-ui` → **Dashboard**.
2. Reload the Home Assistant browser (Ctrl+Shift+R). **Do not restart HA Core.**
3. Open the existing **Evohome** sidebar, or add a Lovelace card using the
   JavaScript module resource
   `/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js` and
   `type: custom:evohome-dashboard-card`.

For frontend-only updates, update this HACS Dashboard package then reload the
browser. HACS serves `/hacsfiles/` content without browser caching.

## Compatibility and safety

The Python backend retains the authenticated WebSocket API, cloud state,
heating commands, and account secrets. The frontend contains **only UI**.
If the optional frontend is not installed, the backend's v0.2 bundled legacy
UI is used automatically.

If you added the older Lovelace resource
`/api/evohome_dashboard/dashboard.js?v=0.2.0`, replace it with the
`/hacsfiles/` resource to avoid loading both.

**Physical heating safeguards do not change with UI-only releases.** Backend
module/Resideo-library upgrades still require a Home Assistant Core restart.
The host computer does not need rebooting.

This directory is a **publishing template**, not an independent HACS repository
while nested in the backend. Publish its files at the root of a dedicated
public GitHub repository named `evohome-dashboard-ui`.
