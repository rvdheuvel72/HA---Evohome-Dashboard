/* Evohome Dashboard: choose an external HACS UI only when feature-compatible.
 * Do not execute an older external module: it can register the custom elements
 * first and hide features that exist in the bundled integration frontend.
 */
const FRONTEND_URL = "/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js";
const REQUIRED_FEATURES = [
  "/* EVOHOME_UI_FEATURE: heating-demand-v1 */",
  "/* EVOHOME_UI_FEATURE: backend-version-label-v1 */",
];
const backendVersion = new URL(import.meta.url).searchParams.get("v") || "local";
const BUNDLED_URL = "./dashboard-legacy.js?v=" + encodeURIComponent(backendVersion);

let externalCompatible = false;
try {
  // Feature-check the source *before* executing its customElements.define().
  // A 404, an old HACS module or an HTML error page means use the bundled UI.
  const response = await fetch(FRONTEND_URL, {cache: "no-store"});
  if (response.ok) {
    const source = await response.text();
    externalCompatible = REQUIRED_FEATURES.every(marker => source.includes(marker));
  }
} catch {
  // The standalone HACS frontend is optional.
}
if (externalCompatible) {
  try {
    await import(FRONTEND_URL);
  } catch {
    console.warn("Evohome Dashboard: external UI failed, using bundled UI.");
    await import(BUNDLED_URL);
  }
} else {
  console.info("Evohome Dashboard: external UI missing or outdated; using bundled UI.");
  await import(BUNDLED_URL);
}
