# R8810A OpenTherm compatibility scan (experimental v0.4.0)

## Scope

The existing receive-only gateway continues receiving generic RAMSES-II frames.
The new OpenTherm parser recognizes RP/3220 replies, parity, Data-ID, message
type and a constrained set of engineering-unit values. Replies from the
configured R8810A are published on the additional MQTT topic
RAMSES/GATEWAY/18:NNNNNN/opentherm/event.

The optional scanner sends only RQ/3220 OpenTherm Read-Data requests. No
RAMSES W packets, OpenTherm Write-Data, actuator commands, pairing,
setpoints or boiler reset functions are implemented.

**Hardware RF transmit has not yet been verified with a real CC1101 and R8810A.**
The feature is disabled by default.

## Radio frequency

Set GW_CC1101_MODULE_BAND = 868 and GW_CC1101_FREQUENCY_KHZ = 868300
in GatewaySecrets.h (integers, frequency unit kHz). The code computes
CC1101 FREQ2/FREQ1/FREQ0 from a 26 MHz crystal and checks that the frequency
falls in the declared band. The CC1101 silicon supports multiple ranges,
but module matching components and antenna may only support one band.
Confirm the actual module hardware before changing any setting.

For Evohome RF **transmission** the firmware enforces exactly 868300 kHz,
and only for the 868 MHz module. Other frequency settings are RX-only.

## Configure (private GatewaySecrets.h)

GW_OT_ENABLE_RF_TX = 0 by default. Only after RF hardware and reception
are confirmed, set it to 1, together with:
- GW_OT_BRIDGE_ID: decimal digits from the actual 10:xxxxxx bridge RF address.
- GW_OT_SOURCE_ID: decimal digits from a legitimately unused 18:xxxxxx
  gateway RF address in range 1..262142 (except reserved 730).
  This is NOT the MQTT device ID.
- GW_OT_SCAN_ALL_256: 0 probes the 28 curated Read-Data IDs; 1 scans
  all numbers 0..255 (including IDs that are not meaningful as Read-Data).
- GW_OT_AUTO_POLL: 0 by default, or 1 for optional compatibility-derived
  polling only after the profile has been populated.

An example ID in documentation is NOT authorization to reuse an occupied
device identity. RF traffic should be tested at a low rate on a trusted
local installation. The default scan makes one Read-Data request every
five seconds and waits 1.8 seconds for a matching response.

Configure a non-placeholder private GW_DEBUG_USER and GW_DEBUG_PASSWORD.
HTTP Basic sends credentials in plaintext; use a trusted local network
and never forward the web port to the Internet.

## Run

Open the local gateway web diagnostics UI and use the
**Start read-only test** button, or POST to /api/v1/opentherm/scan
with HTTP Basic credentials. Read the live profile using
GET /api/v1/opentherm/profile.

The profile is saved to NVS when the scan completes. It is keyed to the
configured R8810A RF ID and CC1101 frequency and restored after reboot.

Results: read_ack (valid reply), data_invalid (recognized but not usable),
unknown_data_id (not recognized in this bridge/boiler path), timeout
(no correlated valid reply), and not_tested.

Only IDs with genuine parity-valid Read-Ack from the expected RF bridge
and addressed to the configured test identity can be marked supported.
No reply does not prove lack of boiler support. Scanning arbitrary
write-only or parameterized OpenTherm IDs with a zero-value Read-Data
request cannot reveal all firmware capabilities.

Optional auto-poll is constrained to documented, reviewed data IDs
acknowledged in the scan, with at most one RF request per minute.

## Important limits

- This is not a general-purpose RAMSES II write/pairing or zone controller:
  the strict no-write requirement intentionally excludes these functions.
- CC1101 TX timing, UART polarity, RF receive recovery, channel access and
  bridge behavior still require real hardware acceptance tests.
- The system continues exposing raw RAMSES RX and existing MQTT Discovery
  gateway diagnostics. The new OpenTherm MQTT event is not yet an automatic
  Home Assistant discovery sensor.
- The OpenTherm profile is observed compatibility evidence, not a complete
  certification of the boiler or a promise that each measurement will work
  under all operating conditions.
