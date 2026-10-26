# RAMSES Arduino Gateway 0.4.0-beta.33

This experimental release adds a guarded **manual read-only R8810A** data path,
without binding a second controller. It reuses the identity of the
physically observed Evohome controller to send one `3220` OpenTherm
Read-Data request to the verified R8810A.

### Features
- Device identities MUST be observed over checksum-valid RF from a real
  controller-to-bridge message; indirect address inference is insufficient.
- Allowlisted IDs only: modulation (17), pressure (18), DHW flow (19),
  boiler flow (25), DHW temperature (26), outdoor temperature (27),
  boiler return (28), exhaust (33).
- No automatic reading, retry, scan, pairing or writes; one manually authorized
  request per 60 seconds, with a short fixed response timeout.
- GET `/api/v1/opentherm/paired-read` reports live state, raw and
  converted reply, RX correlation, and local radio TX result.
- POST requires strong web authentication and an explicit same-origin
  confirmation; normal pairing and sensor functions are unchanged.
- New native host tests for payload, destination, OpenTherm message type,
  response identity, parity, timeout, cooldown and rollover safety.

### Safety and limitations
Physical real Evohome-to-R8810A binding stays intact. A deliberate
controller-address duplicate can create interference; the RF reply isn't
cryptographically attributable to our particular request, even if source,
destination, opcode and requested Data ID match the outstanding probe.
Successful compilation does not prove that the real R8810A responds:
physical testing is required. Do not reset the bridge.

Previous beta.32 12: thermostat sensor pairing, temperature reporting,
local-only stop/forget and both 01: and 12: virtual-role features are kept.
