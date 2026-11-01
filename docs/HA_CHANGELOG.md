# Evohome Dashboard Home Assistant releases

This changelog is **only for the HACS Home Assistant integration**.
ESP32/RAMSES Arduino firmware has separate beta release tags and changelogs.

## 0.3.6

- Reuse existing Resideo cloud room names (and stable cloud zone IDs) in the
  Heating demand tab. RF zone cards now have room selection dropdowns with
  the actual configured Evohome rooms; no manual JSON room-name entry needed.
- Decode controller RAMSES `0004` zone-name frames passively and automatically
  associate RF zone indexes with cloud rooms when the reported name is an exact
  unique match. Unobserved, missing, or ambiguous names remain explicitly
  unlinked instead of guessing cloud list order.
- Save manual selection as a stable RF-to-cloud-zone link in HA options, not
  merely a copied room label. Cloud room renames will then reflect the new
  name, and links persist across restarts.
- Preserve old `rf_zone_names` mappings for backwards compatibility, but prefer
  a cloud link when explicitly selected. No ESP32 firmware change, RF
  transmission, pairing, or heating control is required.

## 0.3.5

- **Automatic HACS update detection:** successful reviewed, merged and
  CI-validated `main` changes now automatically publish a stable GitHub
  Release tagged with the version of the HA integration manifest.
- Prevent integration runtime changes from merging without an increased
  `custom_components/evohome_dashboard/manifest.json` semantic version.
  Matches the release-version discipline used in Hyundai Battery Insight.
- Keep ESP32 firmware prerelease tags independent; the HA release only
  includes files from this repository's reviewed merge commit.
- Includes previous 0.3.4 UI fixes: the Heating demand tab remains available
  even when an older independent HACS UI module was registered, and the
  dashboard shows the installed HA backend version and UI source.
- **Upgrading:** existing installations that previously tracked a commit
  may require one final manual switch to stable version `v0.3.5` in HACS.
  Subsequent stable releases are expected to generate ordinary update
  notifications. Home Assistant Core restart is required after installing
  Python integration updates.

## 0.3.4

- Show live backend version and bundled/HACS frontend source in the dashboard.
- Prevent stale independent HACS UI modules from hiding Heating demand.
- Preserve current sidebar even when a stale Lovelace card registered first.

## 0.3.3

- Migrate deprecated `via_device` links to registry-backed `via_device_id`.
- Retain tested evohome-async 2.1.0 and Node 24 until their major upgrades
  complete compatibility validation.

## 0.3.2

- Add read-only RAMSES-II room demand monitoring through the existing
  gateway MQTT RX topic, including zone demand, estimated request minutes
  and OpenTherm bridge traffic observations.
