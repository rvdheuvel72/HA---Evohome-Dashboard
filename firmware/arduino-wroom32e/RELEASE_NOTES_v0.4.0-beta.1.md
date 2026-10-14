# RAMSES II Gateway v0.4.0-beta.1 — ESP32-WROOM-32E

**BETA / NOT VALIDATED ON PHYSICAL RADIO HARDWARE.** The firmware has been compiled and unit-tested, but the OpenTherm RF diagnostic-transmit path has not been tested with an actual CC1101, Honeywell R8810A and Intergas boiler.

## Included
- Existing web diagnostics, hostname/mDNS, MQTT RAMSES RX, and HA gateway monitoring.
- CC1101 tuning with physical module-band validation (868300 kHz default).
- Passive RAMSES II `3220` OpenTherm response decoding.
- Explicitly gated **Read-Data only** OpenTherm probe, curated scan or optional 256-ID read sweep.
- Per-ID compatibility statuses and persistent NVS compatibility profile.
- Automatic recommendation of safe read polling IDs from observed `Read-Ack` responses.
- Browser diagnostic scan and JSON export: `/api/v1/opentherm/profile`.

## New gateway configuration and diagnostic timestamps

- Readable absolute date and time for every diagnostic-log entry, with
  configurable timezone and CET/CEST daylight-saving changes.
- Edit Wi-Fi, MQTT, hostname, timezone, recovery AP password, web login,
  CC1101 tuning and guarded OpenTherm read-only settings in the web interface.
- Settings saved to ESP32 NVS, so further changes do **not** require Arduino
  recompilation. The compiled GatewaySecrets.h supplies initial defaults only.
- Wi-Fi changes are staged and rolled back automatically if the new network
  remains unavailable for 90 seconds.
- Password fields are write-only (not returned by the API); saves require
  strong existing web authentication and a same-origin request header.
- The CC1101 may be physically disconnected: the gateway and configuration
  page work without it. RF scanning stays disabled until a real bridge and
  correctly wired radio are confirmed.

## Safety
RF transmission is **disabled by default**. A proper 868 MHz CC1101 module, an observed RF bridge, non-conflicting virtual gateway ID, explicit configuration and HTTP authentication are required. **No OpenTherm Write-Data, RAMSES W, boiler resets, pairing or heating control** is implemented.

The compatibility scan is an experiment, not a guarantee that every Data-ID can be read, nor a certification of this boiler. Changing the CC1101 frequency cannot turn an 868 MHz-only RF module or antenna into a multiband device.

## Install
Download **RAMSES-Arduino-Gateway-v0.4.0-beta.1-ESP32-WROOM-32E.zip** from the **Assets** section below. Verify the accompanying SHA256 file. Unzip and open `RamsesGateway/RamsesGateway.ino` in Arduino IDE, copy `GatewaySecrets.example.h` to private `GatewaySecrets.h`, and follow `README.md` and `READ_ONLY_OPENTHERM.md`.

A compiled firmware set for this ESP32 board is included under `firmware-bin/`; use Arduino IDE upload for the documented development workflow.

**Known limitations:** direct hardware verification, radio transmit collision avoidance, reliable OT reply correlation, actual Intergas compatibility and Home Assistant auto-discovery of OpenTherm entities are not yet acceptance-tested. Existing HA discovery of gateway diagnostics remains available.
