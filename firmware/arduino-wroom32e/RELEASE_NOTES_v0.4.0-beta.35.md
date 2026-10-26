# RAMSES Arduino Gateway 0.4.0-beta.35

## Fix: R8810A CV read buttons not responding

Beta.34 could keep the manual R8810A ID25 and eight-value CV read buttons
disabled because it required a directly received 01: Evohome-to-R8810A
request during the current gateway boot. This is not always observed, even
when the receiver decodes valid broadcasts from both devices.

* For manually authorized read-only RF diagnostics, discover the existing
  Evohome Display `01:` identity from checksum-valid controller broadcasts
  and use that identity as the source of each `3220` Read-Data request to
  the configured `10:` R8810A bridge. No additional 01: pairing required.
* Do not transmit if multiple 01: identities are observed, if the bridge's
  controller evidence conflicts with the Display source, or if the radio
  configuration or RF quiet-period checks fail.
* The eight-value batch fixes one `01:` source identity for its lifetime.
  Individual requests and batch requests remain one-at-a-time, manually
  confirmed, with a minimum 60-second cooldown and no automatic retries.
* The web dashboard reports exactly why a read button is unavailable
  (for example: bridge not yet observed, display not yet observed, RF
  cooldown, source conflict, radio configuration).
* Explicit server-side rejections remain visible instead of being erased
  by the next CV status poll.
* The normal Evohome-source single-read control is displayed before the
  separately identified **legacy** gateway-`18:` one-shot RF experiment.
* Native regression tests cover broadcast source selection, conflicts,
  and the eight-request batch's immutable source identity.

The CV scan still reads only ID 17, 18, 25, 28, 5, 33, 57, and 26,
excluding zone temperatures and uninstalled outside-sensor ID27.

**Safety and limitations:** These are OpenTherm Read-Data queries only.
No heating-control writes, binding changes, or periodic polling are added.
Reusing the real Evohome Display's RF source address can collide with the
physical controller's transmissions. Even local TX completion and a
matching bridge reply do not prove causal RF delivery. Actual radio
behavior must be checked on the user's ESP32/R8810A hardware.
