# Passive RAMSES room heat-demand monitoring (backend 0.3.2)

## Purpose and confidence

This optional feature consumes the existing ESP32 CC1101 gateway's published RAMSES-II
packets via Home Assistant MQTT. **It never sends RF packets or writes boiler
settings.** No ESP32 firmware upgrade is required as long as the gateway
publishes timestamped JSON to its existing \`.../rx\` topic.

| Reading | Interpretation |
| --- | --- |
| TRV \`3150\` | Observed demand (half-percent units) for a controller zone index |
| Controller \`0008\` | Controller-generated control demand indication; **not** confirmed boiler firing |
| Bridge \`3EF0\` / \`3220\` | RF exchanges with an OpenTherm bridge; **not** measured burner power |
| Demand duration | Estimate from sample-and-hold observations bounded by a 25-minute TTL |
| Demand-weighted minutes | Time weighted by percentage demand, **not** kWh or m³ gas |

Heat requests from several valves in one zone are aggregated with the highest
fresh demand percentage. For example, two valves in a shared living-room zone
do **not** each count as independent rooms. If we miss reports, the reading
becomes **unknown/stale**, not zero, and we stop counting after the freshness
limit. The displayed duration is consequently an estimate, not a validated
on/off history. RF zone indices are **not** automatically guessed from the
order of zones returned by the Resideo cloud API.

## Configure Home Assistant

1. Confirm the **Home Assistant MQTT integration** is working and connected
   to the *same broker* used by the ESP32 RAMSES gateway.
2. In **Settings → Devices & services → Evohome Dashboard → Configure**,
   set \`rf_controller_id\` to your actual controller address (for example
   \`01:245914\` on this installation).
3. Set \`rf_mqtt_topic\` to the gateway's MQTT RX topic (for example
   \`RAMSES/GATEWAY/18:447669/rx\`). The default wildcard
   \`RAMSES/GATEWAY/+/rx\` is also supported, with every packet still checked
   for the configured controller address.
4. Initially leave \`rf_zone_names\` as \`{}\`. Allow packets to arrive, then
   inspect **Heating demand** for detected RF zone indices and source TRV IDs.
   Explicitly map verified zone indices to real rooms, e.g.:

   \`\`\`json
   {"00": "Living room", "03": "Bedroom 1"}
   \`\`\`

   **These zone numbers are illustrative and must be verified against your
   real installation.** Reopen integration options and save the mapping;
   Home Assistant reloads the integration.

5. Check the **Heating demand** tab in the Evohome sidebar or the new Home
   Assistant RF sensors and their History graphs. Starting with backend
   **0.3.4**, the dashboard loader checks whether an independently installed
   HACS frontend supports `heating-demand-v1` before loading it. When the
   optional frontend is missing or older, it loads the bundled implementation
   **with** Heating demand instead of showing an outdated frontend.
   **Restart Home Assistant Core after the HACS integration backend update,
   then hard-refresh the browser** so the new panel JavaScript module is used.
   An HACS download alone does not reload the Python integration.

Once enabled, Home Assistant creates a controller RF-demand sensor, last
OpenTherm bridge RF activity sensor, and a **demand (%)** and **estimated
request minutes today** sensor for every explicitly mapped or newly observed
RF zone. Each zone sensor includes all reporting TRV device IDs as attributes.

The monitor stores a bounded daily summary, batching disk writes every five
minutes and saving on integration unload. On restart, previous duration totals
can be restored, but previously observed demand is deliberately **not**
resurrected as a live reading. A short portion of the most recent unflushed
totals can be lost after unexpected power loss.

## Out of scope / not measured

- No conversion from demand (%) to room gas use (m³), kWh, valve opening or
  boiler modulation. A P1 meter's whole-house readings cannot prove which
  room consumed a specific volume.
- No assumption that any single room **caused** an individual boiler firing.
  Multiple zones can be requesting simultaneously, and OpenTherm controller
  behaviour may optimize or defer firing.
- The supplemental underfloor loop and manual radiators are not individually
  visible through a TRV heat-demand packet.
- No automatic RF zone-name discovery, active probing or pairing.
- No cross-day permanent report database beyond Home Assistant Recorder; daily
  counters are restored for the current day only.

## Diagnostic validation before trusting reports

1. Capture several real \`3150\` packets while a known room changes setpoint.
2. Check that each source TRV ID corresponds to the intended room and index.
3. Test the two living-room valves as a *single* logical zone.
4. Compare controller \`0008\`, OpenTherm bridge messages and P1 readings on
   a common timeline. Do **not** treat those three measurements as identical.
5. Inspect the gateway's rejected/dropped counts; RF packet loss is not
   distinguishable from silence in the basic room-demand history.

Protocol references: [RAMSES protocol](https://github.com/ramses-rf/ramses_protocol)
and [ramses_rf](https://github.com/ramses-rf/ramses_rf).

## If the Heating demand tab is missing

- In HACS, redownload **Evohome Dashboard (Integration)** from `main`.
- Restart **Home Assistant Core** once (not just HACS).
- Hard-refresh the browser or open a new private/incognito tab. Browsers
  keep already-registered custom elements for the duration of an SPA page.
- If you have a separately installed older HACS **Dashboard** UI, backend
  0.3.4 automatically uses its own current bundled dashboard instead of that
  old JavaScript module. This compatibility fallback is deliberately silent
  about RF data availability: seeing the tab does not prove MQTT capture.
- The dashboard header displays the actual installed **HA backend v0.3.4**
  (read from the integration manifest) and **UI: Bundled** or **UI: HACS**
  according to the module loaded. A version mismatch therefore remains visible.
- If the tab appears but says monitoring is disabled, set the RF controller
  address and MQTT topic in the Evohome integration options.

## Preloaded old HACS resources

Home Assistant 0.3.4 uses the integration-owned sidebar element name
`evohome-dashboard-ha-panel`. This prevents an older independently installed
Lovelace HACS resource which already registered `evohome-dashboard-panel`
from blocking the current bundled sidebar UI. The same-name old Lovelace
`custom:evohome-dashboard-card` can still display old content until its HACS
resource is updated; the fix specifically guarantees the **Evohome sidebar**.
