# RAMSES Arduino Gateway 0.4.0-beta.34

## New: one-click CV/boiler measurements

The Diagnostics tab has a **Read CV data** button and results table. A deliberately
authorized action queues up to **8** serial OpenTherm Read-Data requests addressed
to the previously observed R8810A, reusing the observed 01: Evohome identity.

The fixed allowlist is: ID17 (relative modulation), ID18 (CH water pressure),
ID25 (boiler flow), ID28 (boiler return, flagged unverified), ID5 (raw boiler
fault/service flags), ID33 (exhaust/flue temperature), ID57 (maximum CH
temperature setpoint) and ID26 (DHW temperature). Zone/room temperatures and
ID27 (outdoor, no physical sensor) are excluded.

* **One RF request at a time; minimum 60-second spacing.** Each ID is attempted
  once and only once. The gateway aborts if the next safe RF window remains
  unavailable for more than 120 seconds. No background periodic scan.
* Each Read-Ack is checked for source, destination, data ID and OpenTherm parity.
  Negative answers and timeouts are shown, not turned into zero readings.
* Status lives on the ESP32, and can be viewed again after closing/reloading the
  browser. Results and scan progress are RAM-only, reset on reboot/new click.
* UI displays local TX completion separately from the observed Read-Ack.
  A matching reply does NOT prove the gateway's particular duplicate-address
  request caused the response.
* Existing manual single ID read, virtual room thermostat, experimental
  pairing, inbound RF logging, MQTT and configuration remain available.
* Native tests exercise 60-second pacing, duplicate start protection, timeout,
  no automatic retries, valid read IDs and zero writes.

**Caution:** Identity reuse can collide with the real Evohome's RF traffic.
The user's real Evohome/R8810A pairing is not reset or modified. An observed
value is not automatically a calibrated or physically credible boiler reading.
