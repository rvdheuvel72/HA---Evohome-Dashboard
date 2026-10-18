# RAMSES Arduino Gateway v0.4.0-beta.16 — R8810A controller recognition fix

## Symptom and evidence

On 2026-10-18 the ESP32 received a decoded R8810A message:

```text
104  I --- 10:048994 --:------ 01:245914 3150 002 FC00
```

The beta.15 pairing preflight continued to say `evohome_controller_not_observed`, because `RamsesPeerDetection.h` only learned a controller from a directly observed `01: → 10:` request or a `3EF0 RP` addressed to the `01:` controller in address slot 2. This real `3150 I` status placed the controller in address slot 3 and did not match either path.

## Changes

- Passive controller inference now accepts strictly matched `3150 I` status from the **configured** `10:` bridge, with the `01:` controller in address slot 3, expected address mask, length 2, and `FC` domain.
- Existing explicit `01: → 10:` controller requests take precedence over this evidence, which in turn takes precedence over provisional `3EF0 RP` inference.
- **Codex P2 addressed:** conflicting `3150 I` controller targets (e.g. a retained virtual controller and the real Evohome) now make identity **ambiguous** and block any new pairing. The API exposes `controller_ambiguous` and `block_reason=evohome_controller_ambiguous`; only a subsequently received direct controller request to the configured bridge can resolve it. Negative and recovery tests cover both arrival orders.
- Negative tests reject wrong bridge, radiator sender, invalid message verb/addresses/opcode/domain/length, and a non-controller in address slot 3.
- A received `3150` does not prove the bridge supports a second controller, and does not validate over-the-air transmit performance.

## Safety and validation

**Receive-only fix.** This release adds no RF transmissions, pairing retries, binding writes, resets, heating commands, or changes to the 1FC9 packet vectors. The separate experimental pairing feature still requires strong authenticated manual authorization, unseen virtual controller ID, previously observed R8810A, frequency configuration, and its existing single-shot guards and cooldown.

Run the exact-head host-native C++ test suite, Arduino ESP32 compile, JS/release contract checks, and Codex P1/P2 review. Resolve actionable P1/P2 items before merge or publication. Real over-the-air dual-controller pairing with the R8810A remains unverified; **do not erase the original Evohome binding**.

Once installed, wait for a fresh `3150 I` packet from the configured R8810A and inspect `GET /api/v1/opentherm/pairing`. The `existing_controller_id` should become `245914`; if not, capture diagnostics before attempting any RF command.
