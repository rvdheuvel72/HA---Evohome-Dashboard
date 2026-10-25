# RAMSES Arduino Gateway v0.4.0-beta.28 — verified additional-controller binding fix

## Concrete pairing bug fixed
An R8810A OpenTherm `3220` reply to the virtual `01:` controller
previously set the binding state to `VERIFIED` whenever the payload parsed as
a Data-Invalid, Unknown-DataId or Read-Ack response. This mistakenly
authorized use of the virtual controller even when the ID25 read was refused.

Beta.28 transitions to `VERIFIED` **only** after a correctly addressed,
parity-valid OpenTherm **Read-Ack** for ID25. A corresponding Data-Invalid or
Unknown-DataId is recorded as `verification_result`, leaves
`verified_reply=false`, `read_ack=false`, and ends in
`confirmation_sent_unverified`. A timeout remains unverified.

## Safety and diagnostics
- Reject foreign/late 1FC9 accept messages, mismatched controller identity,
  and other existing forbidden state transitions exactly as before.
- Report `verification_result` in the authenticated binding API and web
  pairing panel for terminal negative replies.
- Native regression tests exercise parity-correct Read-Ack, Data-Invalid and
  Unknown-DataId replies and preserve other pairing state-machine contracts.
- Preserve manual-only authorization, distinct unused `01:` virtual ID,
  60-second cooldown, read-only verification, and original Evohome binding.
- Do not automatically transmit, enter R8810A pairing mode, erase bindings,
  reset the boiler or enable periodic requests.
- Preserve RX fixes from beta.26 and guarded unpaired `18:` ID25 test from
  beta.27, unchanged.

## Validation
Self-hosted Arduino build and C++ test suite, trusted exact-head Arduino compile,
Codex P1/P2 check, release-readiness review and immutable new beta tag.

Physical over-the-air delivery, a successful new R8810A additional-controller
binding, and coexistence with the existing controller remain to be verified
on the actual heating installation.
