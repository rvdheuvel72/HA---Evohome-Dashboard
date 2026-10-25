# RAMSES Arduino Gateway v0.4.0-beta.29 — controller and virtual wireless thermostat

## Both RF roles retained
- **Additional OpenTherm controller (01:)**: Keep existing manual R8810A
  `1FC9` offer, acceptance, confirmation, guarded `3220` Read-Data ID25
  verification and original Evohome controller protections.
- **Virtual room thermostat (12:)**: Separate `1FC9` pairing offer targeted
  at the *existing* Evohome controller, not the R8810A. Match a received
  controller W acceptance to the exact device ID, target, domain `2309`,
  controller ID and assigned zone, then send confirmation. Once the
  controller has accepted, allow `30C9` zone temperature reports.

Temperature input can come from the authenticated web API or MQTT
`RAMSES/GATEWAY/<gateway-id>/thermostat/temperature/set`; publish signed
integer hundredths of °C, e.g. `2150` = 21.50 °C. No automatic RF
pairing; measurements must be fresh (under 15 minutes), reports are no
faster than every 5 minutes. At boot no virtual thermostat pairing is
replayed from storage; experimental session must be bound anew.

## Safety and coexistence
The virtual 12: sensor and 01: controller have independent session state.
Both use the single dedicated radio task and never intentionally overlap
TX. Neither sensor temperature transmission nor thermostat binding invokes
boiler writes or clears existing R8810A bindings. The separate 01: role
remains experimental: additional controller coexistence with an already
bound Evohome has not been proven on real hardware.

## Obsolete bring-up cleanup
Delete the GPIO26→GPIO25 UART-only bench loopback handler, fixture header,
web controls, and host test. Keep the passive RF capture, SPI and RX
diagnostics, and guarded OpenTherm ID25 read, which also helps diagnose
real controller-to-bridge protocol responses. Native CI still verifies
controllers, thermostat frames and timeouts.

## Verification
Self-hosted Linux protocol tests and ESP32 builds, trusted exact-head
Arduino compile, Codex P1/P2 review, pre-merge gate and post-merge release
readiness are mandatory. Correct API/software behavior does not establish
that the physical M5Stack radio has successfully radiated a valid frame.
Physical Evohome acceptance and subsequent valid temperature reception
remain unverified until observed in the user's actual installation.
