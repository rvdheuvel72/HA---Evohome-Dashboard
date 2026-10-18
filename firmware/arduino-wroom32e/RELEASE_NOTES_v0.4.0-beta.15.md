# RAMSES Arduino Gateway v0.4.0-beta.15 — Correct OpenTherm R8810A 1FC9 offer

## Protocol defect fixed
Previously, the experimental additional-controller offer advertised a 3-entry heating-valve relay capability list (`00|0008, FC|3B00, 00|1FC9`). The recorded OpenTherm bridge pairing exchange instead advertises **four** entries in a 24-byte `1FC9` payload: `FC|0008`, `FC|3150`, `FB|3150`, and `FC|1FC9`. All four entries carry the virtual controller's RAMSES RF address. The encoder now produces the correct 35-byte packet (24-byte payload + header/addresses/opcode/length/checksum), including a recomputed checksum.

This matches the **independent real controller → OpenTherm bridge capture** in [ramses-rf/ramses_rf `code_1fc9.log`](https://github.com/ramses-rf/ramses_rf/blob/master/tests/tests_rf/data_driven/parsers/code_1fc9.log) dated 2020-11-23 (controller `01:078710`, bridge `10:067219`). The same trace documents the acceptance `1FC9 W 006 003EF0...` and the final confirmation `1FC9 I 006 00FFFF...`, whose structure was already implemented.

## Diagnostics and validation
- The manually triggered **GPIO26 → GPIO25 bench loopback** now sends **82 UART bytes**, not 70, at 38400 baud, 8N1. It continues to keep CC1101 in receive mode and never issues an RF STX command.
- Host-native protocol tests assert all four zone/code/address triplets and compare **exactly** against two independently specified complete packet vectors, including reconstructed checksum from the upstream OTB capture.
- Host-native loopback tests cover 82-byte valid capture/decoder, insufficient buffer, truncation, mismatch, extra data and bounded hexadecimal formatter.

## Safety and limitations
- **No automatic binding attempts or retries.** Existing explicit manual RF authorization, ID checks, single-attempt state machine, radio restrictions, one-shot guard and cooldown are preserved.
- Flashing the firmware or running the **GPIO-only** loopback test does not transmit RF or alter any binding.
- A recorded 1-controller-to-OTB exchange does **not** prove the R8810A can retain two independent controllers concurrently. Do not clear/reset the existing Evohome ↔ R8810A binding. Further on-air testing is experimental and only upon explicit user authorization; UART loopback alone cannot verify RF radiation, antenna output or remote reception.
