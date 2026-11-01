# R8810A Diagnostics inside Evohome Dashboard (HA v0.3.7)

This is a Home Assistant integration feature. It does **not** update the ESP32
firmware, change Evohome setpoints or require a desktop Python script.

## One-time setup

1. Install Evohome Dashboard v0.3.7 (after it is published), restart Home
   Assistant Core and hard-refresh the browser.
2. In Home Assistant, open **Settings → Devices & services →
   Evohome Dashboard → Configure**. Enter:
   - **RF gateway host**: \`evohome-gateway\` or the ESP32's **private IPv4**
     address such as \`192.168.1.x\`. Do not include \`http://\` or a port.
     Prefer the IP address if the HA host cannot resolve the local hostname.
   - **RF gateway user** and **RF gateway password**: the separate HTTP Basic
     authentication credentials used for the ESP32 web dashboard.
3. Open the left sidebar **Evohome → R8810A test**. The HA backend communicates
   with the local ESP32: there is no browser-to-gateway CORS/mixed-content
   dependency. The password is never returned to the browser.

## Running the test

- If there is a previous capture, click **Download existing gateway capture**
  first to preserve it. The test refuses to overwrite an existing capture
  unless the separate **Replace gateway packet capture** checkbox is checked.
- Tick **I authorize read-only RF requests** and choose **Test ID25**
  (boiler flow °C) or **Run IDs 18, 25, 117**.
- Confirm the browser authorization dialog. The ESP32 sends at most one
  \`3220 Read-Data\` per selected ID using the physical Evohome controller's
  **observed RF identity**, so RF collisions remain possible. The gateway's
  existing cooldown and preflight checks still apply; the HA integration does
  not retry, pair, bind, or transmit an OpenTherm write.
- The page remains usable while the background diagnostic runs. Results
  show Read-Ack, timeout, explicit Unknown-DataId, Data-Invalid and local
  TX phase/bytes. Click **Download report.json and capture.tsv** afterward.

## Evidence limitations and security

- **Timeout does not mean unsupported.** Only an explicit reply qualifies as
  Unknown-DataId or Data-Invalid, and these still cannot establish whether the
  bridge or boiler produced the response.
- The capture records **received RAMSES packets**, not the gateway's own RF
  transmission, nor the OpenTherm signal on the wire. The physical Evohome
  controller and diagnostic use the same source identity, so even a matching
  received reply does not guarantee causation.
- The RF decoder validates nested OpenTherm parity and matches the complete
  controller and bridge RF addresses, excluding unrelated nearby devices.
- Backend access is restricted to **Home Assistant administrators** and only
  to a private LAN IPv4 or the two supported gateway hostnames. Redirects
  are forbidden. Separate gateway HTTP credentials live in the HA integration
  options (and therefore in privileged Home Assistant configuration/backups);
  use a trusted LAN and protect backups. Credentials and passwords never appear
  in the diagnostic WebSocket results or report.
- The ESP32 uses plain HTTP on its LAN. Authentication is not encrypted on
  that connection. Do not expose the gateway port directly to the Internet.
- Existing ESP32 firmware **v0.4.0-beta.37** already implements the required
  single-read and RAM-only capture HTTP endpoints.

## Versioning

This change belongs to the **Home Assistant integration only**, not the
RAMSES Gateway firmware. The two release channels currently share the same
GitHub repository; publishing HA stable releases can become GitHub's "Latest"
ahead of firmware beta downloads. The release-channel separation discussed
in the project is a separate unresolved deployment/design issue.
