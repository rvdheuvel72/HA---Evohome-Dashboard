# Evohome Dashboard Home Assistant releases

This changelog is **only for the HACS Home Assistant integration**.
ESP32/RAMSES Arduino firmware has separate beta release tags and changelogs.

## 0.3.5

- **Automatic HACS update detection:** successful reviewed, merged and
  CI-validated `main` changes now automatically publish a stable GitHub
  Release tagged with the version of the HA integration manifest.
- Prevent integration runtime changes from merging without an increased
  \`custom_components/evohome_dashboard/manifest.json\` semantic version.
  Matches the release-version discipline used in Hyundai Battery Insight.
- Keep ESP32 firmware prerelease tags independent; the HA release only
  includes files from this repository's reviewed merge commit.
- Includes previous 0.3.4 UI fixes: the Heating demand tab remains available
  even when an older independent HACS UI module was registered, and the
  dashboard shows the installed HA backend version and UI source.
- **Upgrading:** existing installations that previously tracked a commit
  may require one final manual switch to stable version \`v0.3.5\` in HACS.
  Subsequent stable releases are expected to generate ordinary update
  notifications. Home Assistant Core restart is required after installing
  Python integration updates.

## 0.3.4

- Show live backend version and bundled/HACS frontend source in the dashboard.
- Prevent stale independent HACS UI modules from hiding Heating demand.
- Preserve current sidebar even when a stale Lovelace card registered first.

## 0.3.3

- Migrate deprecated \`via_device\` links to registry-backed \`via_device_id\`.
- Retain tested evohome-async 2.1.0 and Node 24 until their major upgrades
  complete compatibility validation.

## 0.3.2

- Add read-only RAMSES-II room demand monitoring through the existing
  gateway MQTT RX topic, including zone demand, estimated request minutes
  and OpenTherm bridge traffic observations.
