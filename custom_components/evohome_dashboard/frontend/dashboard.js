/* Evohome Dashboard v0.3: stable backend-to-HACS frontend loader.
 *
 * The UI has its own HACS Dashboard package at:
 *   /hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js
 * HACS /hacsfiles assets are not browser-cached. Reloading the browser picks
 * up frontend-only releases without restarting HA Core or heating entities.
 *
 * If the optional frontend is not yet installed, use the bundled frozen v0.2
 * experience. This maintains backward compatibility across the migration.
 */
const FRONTEND_URL = "/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js";
try {
  await import(FRONTEND_URL);
} catch (error) {
  console.warn("Evohome Dashboard UI unavailable; using bundled legacy frontend.");
  await import("./dashboard-legacy.js");
}
