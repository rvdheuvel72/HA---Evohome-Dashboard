## Beta.21: Preserve confirmed RX and provide UART byte evidence

**Release:** [RAMSES Arduino Gateway v0.4.0-beta.21](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.4.0-beta.21) after CI, Codex review and merge.

The last user-confirmed successful RAMSES packet on the M5Stack M146 was on beta.16 (`sync=1 valid=1`). The decoder and core radio-frequency settings are unchanged through beta.20, but beta.20 shows `initial_MARC=31 RX_at_boot=0`, and no RAMSES sync. Beta.21 stops unconditional 30-second RX re-entry, recovers only after two confirmed consecutive IDLE/overflow readings, and provides **read-only** UART evidence at `/api/v1/radio/uart-probe`. To correlate the radio state and raw receive bytes, query `/api/v1/status`, `/api/v1/radio/rx-health`, `/api/v1/radio/uart-probe`, and `/api/v1/packets` on the authenticated local gateway.

**RF communication is not considered repaired until new valid packets are observed on the physical gateway.** No automated RF TX, pairing, or boiler commands are added.

## Beta.20: Avoid disruptive CC1101 reset and measure web/MQTT slowdown

**Release:** [RAMSES Arduino Gateway v0.4.0-beta.20](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.4.0-beta.20), after required CI and review checks.

**Arduino IDE ZIP:** `RAMSES-Arduino-Gateway-v0.4.0-beta.20-ESP32-WROOM-32E.zip`.

**WebServer upstream repair:** The beta.20 binary is compiled with Arduino-ESP32 **3.3.12**, which includes Espressif's fix for incomplete-header HTTP server hangs (PR #12794). Earlier gateway binaries were built with 3.3.4.\n\nWhen no RAMSES sync is received, the gateway now warns without resetting a previously identified CC1101; an unsolicited radio reset previously could make a working SPI chip appear missing. Status-register reads returning 0xFF are now marked invalid rather than presented as a normal MARC state. The web server is serviced before MQTT, and the status API/serial heartbeat records HTTP/MQTT timing maxima so the slow request source can be identified. Hardware RX reception is still unverified without actual valid RAMSES packets.

## Beta.19: CC1101 false missing after reset fixed

**Release:** [RAMSES Arduino Gateway v0.4.0-beta.19](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.4.0-beta.19) (published only after CI and review).

**Arduino IDE ZIP:** `RAMSES-Arduino-Gateway-v0.4.0-beta.19-ESP32-WROOM-32E.zip`.

Beta.18 could log `detected=0 PARTNUM=0x00 VERSION=0x14` and retry endlessly because its strict 15 ms MARCSTATE check treated a temporary non-RX state as a failed CC1101 chip detection. Beta.19 separates SPI chip detection from RX-state observation, and exposes `cc1101_init_stage`, `cc1101_boot_marcstate` and `cc1101_rx_at_boot` in `/api/v1/status`. The `/api/v1/radio/rx-health` endpoint still measures actual RX status over time. RF TX remains disabled by default.

## Beta.18: Faster web interface and warm ESP32 radio reset recovery

**Release:** [RAMSES Arduino Gateway v0.4.0-beta.18](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.4.0-beta.18) after exact-head CI and required release checks.

**Arduino IDE ZIP:** `RAMSES-Arduino-Gateway-v0.4.0-beta.18-ESP32-WROOM-32E.zip` (published only when release checks pass).

The gateway no longer serves the 41KB debug page as one synchronous HTTP response. HTML and JavaScript are distinct PROGMEM assets, sent in bounded network blocks. When a diagnostic section is closed, its periodic endpoint calls are suspended; log export is still available on demand. Status updates every five seconds. A CC1101 initialization failure after a software-only reboot now retries without needing a permanent USB power-cycle; active RX is checked rather than assuming SPI success.

**Module:** The M5Stack M146 868 MHz board receives 5V on M-BUS 28, via its regulator. Never apply 5V to a bare CC1101 breakout's VCC pin.

**Test:** Refresh `http://evohome-gateway.local/`, then inspect `/api/v1/status` and `/api/v1/radio/rx-health`. Software tests cannot prove stable on-air reception without your R8810A traffic.

## Beta.16: Recognize R8810A 3150 status addressed to existing Evohome controller

The normal Evohome controller can be identified from a **checksum-valid** bridge-originated `3150 I` status packet, even if the receiver misses controller `RQ` messages and `3EF0 RP` replies. Actual diagnostic capture: `I --- 10:048994 --:------ 01:245914 3150 002 FC00`. This address is in the **third address field**, not the second. Passive detection checks the configured bridge ID, `I` verb, `3150` opcode, exact address layout, 2-byte payload and `FC` domain; packets from radiator devices (such as `04:253821`) are not accepted as controller evidence.

A directly observed controller request retains higher priority than inferred status packets. **Codex P2 hardening:** if indirect `3EF0 RP` and/or `3150 I` evidence points to different `01:` controller IDs, pairing now fails closed with `evohome_controller_ambiguous` until a direct Evohome-to-R8810A request identifies the controller. This avoids locking onto an old virtual ID after restart. The change does **not** bypass ID-collision checks, RF frequency limits, authorization, automatic TX disabling, cooldown or manual 1FC9 safeguards. Installing this beta does not transmit RF, put the R8810A into binding mode or change your existing Evohome heating control.

**Release:** [RAMSES Arduino Gateway v0.4.0-beta.16](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.4.0-beta.16), published only after the required CI and review gates succeed.

## Beta.15: Corrected OpenTherm bridge (R8810A) binding offer

**Release:** [RAMSES Arduino Gateway v0.4.0-beta.15](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.4.0-beta.15) — `RAMSES-Arduino-Gateway-v0.4.0-beta.15-ESP32-WROOM-32E.zip`.

The original experimental binding offer was derived from a *heating-valve relay* (0008/3B00/1FC9), not an OpenTherm bridge. We now encode the **observed controller-to-OpenTherm bridge** `1FC9 024` offer: `FC0008`, `FC3150`, `FB3150`, `FC1FC9`, each followed by the controller's RF address. The frame length is **35 RAMSES bytes / 82 encoded 38,400-baud UART bytes**. The final checksum is regenerated for the selected virtual controller. The upstream source is [ramses_rf's actual OTB offer/accept/confirm capture](https://github.com/ramses-rf/ramses_rf/blob/master/tests/tests_rf/data_driven/parsers/code_1fc9.log), dated 2020-11-23, with controller `01:078710` and bridge `10:067219`. Tests compare our output to that independent capture and its checksum, not just to our own decoder.

The **ESP32 GPIO26 → GPIO25 loopback** now sends 82 bytes when you explicitly request that separate **GPIO-only** test. It still performs **no radio STX**, no RF emission, no pairing and no NVS writes. The experimental R8810A RF binding path remains **manual-only**: the existing explicit authorization, controller-ID protection, response validation and 60-second cooldown remain unchanged. No pairing attempt or R8810A reset is initiated by installing the release.

**Safety caveat:** The upstream capture proves a controller-to-OpenTherm binding sequence, but does **not** prove that an R8810A can retain an additional controller while the original Evohome controller remains bound. Do **not** reset the R8810A or assume a second binding cannot displace the original. The payload fix alone cannot prove RF emission or successful communication.

## Beta.14: GPIO26 → GPIO25 electrical UART loopback (no RF transmission)

**Download**: [RAMSES Arduino Gateway v0.4.0-beta.14](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.4.0-beta.14) — `RAMSES-Arduino-Gateway-v0.4.0-beta.14-ESP32-WROOM-32E.zip` (published after exact-head CI and Codex approval).

Use the **ESP32 GPIO26 → GPIO25 jumper** to test the actual UART TX pin electrically. Disconnect CC1101 GDO0 from GPIO26 for this bench test as you have done; CC1101 GDO2/RX and SPI can remain connected. In gateway Diagnostics expand **ESP32 UART TX loopback**, confirm the jumper and click **Test GPIO26 → GPIO25**. It sends one fixed 82-byte Manchester-encoded RAMSES `1FC9` OpenTherm offer *on the wire from GPIO26*, listens with ESP32 UART2 RX on GPIO25, compares every captured byte and checks decoding. **The CC1101 stays in RX and the test NEVER issues an STX strobe, transmits RF, sends a pairing request over RF or changes the R8810A.** No R8810A button press is needed.

View `GET /api/v1/radio/tx-loopback` (authenticated) for `status`, `expected_bytes`, `uart_written_bytes`, `captured_bytes`, `first_mismatch_index`, `decoded_1fc9`, transfer timing, and full `expected_hex` / `captured_hex`. A successful `pass` verifies GPIO26 output, the installed jumper to GPIO25 and UART2 decode; it still **does not prove CC1101 GDO0 reception or radio-frequency transmission**. No new hardware is necessary. Reconnect GPIO26 → CC1101 GDO0 before attempting any future RF command.

The bench test is only available with automatic RF TX, scanning, polling, and in-flight binding/one-shot requests disabled. A separate POST confirmation is required; all activity is RAM-only, one test per click, no automatic repeats.

**Beta.13 release:** [RAMSES Arduino Gateway beta.13](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.4.0-beta.13) · `RAMSES-Arduino-Gateway-v0.4.0-beta.13-ESP32-WROOM-32E.zip`. The release and binary are available only after checks and publishing have completed.

## Beta.13: Verified-local CC1101 TX diagnostics (no new RF commands)

The manual `1FC9` binding attempt now records **live CC1101 SPI register readbacks** for the tuned synthesizer frequency, async-PKTCTRL0, GDO0 IOCFG0, 38.4kbps modem setup and power amplifier, as well as the MARCSTATE during TX, immediately after UART flush and after returning to stable RX. Records expected/written bytes, UART transfer time and minimum expected on-wire UART duration (38400 baud, 8N1). `GET /api/v1/opentherm/pairing` adds `tx_step`, `tx_local_verified`, `tx_local_verification_reason`, raw chip registers, MARC state and timing, and the Diagnostics pairing panel shows them.

The strict `local_tx_verified` verdict means only that the CC1101 register/state and **local UART timing checks** passed. It cannot verify the actual external GPIO26→GDO0 physical connection, RF output at the antenna, protocol compatibility or reception by R8810A. `tx_phase:local_complete` is preserved independently to aid comparison. No extra RF transmissions, retries or bind-mode commands were added; existing safeguards/cooldown and native Evohome binding remain unchanged.

## Beta.12: R8810A bridge-reply controller detection and 5-second binding manual correction

A received checksum-valid `3EF0 RP` addressed **from the configured `10:` R8810A to the original `01:` Evohome controller** now identifies that controller even when the preceding controller request is missed. This is receive-only identity inference; it does not prove an experimental virtual-controller binding or send RF. The R8810A1018 manufacturer's own device installation instructions say **hold its button for 5 seconds** and look for alternating red LED 0.5 seconds on/0.5 seconds off: **they do not require clearing old bindings first**. The previous beta.11 wording suggesting a required 15-second clear was incorrect. The device manual describes a CM957 thermostat and does not prove an additional virtual controller can be bound without disturbing the existing Evohome. The ESP32 **Send one pairing offer** button queues a transaction but does not switch the physical bridge into pairing mode. Do not erase working bindings.

## Beta.11: Pairing button diagnostics and existing-bridge safety

**Diagnostics → Experimental R8810A pairing** now explains **why the button cannot be used** and identifies the exact missing prerequisite: bridge/controller RF observation, radio readiness, preflight configuration, unique 01: ID, authorization, busy state or cooldown. `GET /api/v1/opentherm/pairing` provides `ready`, `radio_ready` and `block_reason`. This does not make an already-bound R8810A accept another controller. **Manufacturer warning:** Resideo's Evohome R8810 setup guide instructs a 15-second *binding erase* before the 5-second bind-mode press. Do not perform that erase just for the ESP32 experiment: it can disconnect normal heating. If 5 seconds does not start the slow red LED blink, stop, gather the diagnostics and retain your working binding.

## Beta.10: Experimental manual 1FC9 pairing with R8810A

**Diagnostics → Experimental R8810A pairing** provides a deliberate, one-attempt additional virtual controller binding (01:). The ESP32 first observes the existing controller and bridge, then offers 1FC9, verifies the R8810A's addressed response, confirms the binding, and sends a single read-only OpenTherm ID25 verification request. It reports **verified** only for a valid reply addressed to its separate virtual controller ID; confirmation transmission alone is **not** proof of pairing. No automatic retries, writes, heating commands or replacement of the existing Evohome controller. **Do not reset the R8810A's bindings with a 15-second press.** This method is unverified on the physical R8810A; use the 5-second bind mode only and check Evohome boiler control after the experiment. Pairing identity is not persisted across ESP32 reboots.

## Beta.9: Switchable RAMSES receive packet capture

**Diagnostics → Extended RAMSES packet capture** now supports **Start new capture, Stop, Clear and Download capture (.tsv)**, without restarting the gateway or changing settings. Capture defaults OFF and records **up to 192 frame events in RAM**. It runs directly inside `radioTask` before the web/MQTT queue, recording every checksum-valid decoded RAMSES frame and every detected malformed/frame rejection with a reason and offending UART byte where available. For valid messages, the original human-readable RAMSES/HGI line and indicative signal-strength value are preserved, with acquisition time (local time including timezone offset, and UTC epoch milliseconds), device uptime, and monotonically increasing sequence. On filling the 192-entry buffer, the capture **stops automatically** (no silent rollover); `/api/v1/capture/status` reports the buffer state, start/stop uptime, decoded/rejected counts, radio UART/sync deltas and main-queue drops. Download from authenticated `/api/v1/capture/download`; no NVS/flash writes, no raw RF bits or IQ and no RF transmit. Unsaved captures are lost on restart or Start/Clear. The old `/api/v1/packets` last-12 display is unchanged.

**Important limitation:** This captures complete RAMSES messages *after* the CC1101 physical receive and UART decoding; it cannot show RF bursts entirely missed by the antenna, radio, UART input, or radio recalibration. Rejected candidates cannot identify their sender. The 192-event bounded memory prevents uncontrolled allocation; stop/clear/export do not disrupt normal RF reception or the 60-second experimental one-shot safeguards. A single snapshot containing `requests_to_bridge` and `bridge_3ef0` numbers alone cannot prove reception losses: the previous 12-packet history may already have overwritten requests.

## Beta.8: Correct upstream FIFO bit alignment, verified byte-for-byte

Independent comparison to `IndaloTech/ramses_esp`'s actual `frame.c::tx_byte()` revealed a genuine beta.7 TX waveform defect: our FIFO encoder sent UART **START=0, 8 data bits, STOP=1** for each byte, but the upstream transmits **MARK=1, START=0, 8 data bits** for each byte. The streams are offset one bit relative to the break primer, which makes the beta.7 first training byte `0x55` versus upstream `0xAA`. The beta.9 experimental FIFO now emits the **exact upstream-aligned 55 RF bytes** after the same `00 FF 00 00` priming bytes and before the `FF` idle byte, still **60 FIFO bytes / one manually authorized RQ3220 ID25 request**. A completely separate, 16-bit shift-register simulation of the upstream routine must match the entire on-air FIFO byte stream, not just be self-consistent. This is a software waveform fix; it **does not confirm that the 433 MHz PCB radiates adequately at 868 MHz, nor that R8810A responds to unpaired 18: identity**.

Unchanged safeguards: manual button, authenticated user, verified R8810A sender, sender-ID collision check, 60s cooldown, no pairing spoofing, no full scans, no polling, no heating writes. The asynchronous UART method is unchanged, and beta.6 RX debug continues to work.

## Beta.7: Test reference FIFO priming (single guarded Read-Data request)

Beta.7 fixes a testable difference between our fixed-length FIFO RF waveform and `IndaloTech/ramses_esp`: the upstream reference primes FIFO with `00 FF 00 00` (startup/break) before the 8N1 RAMSES frame and ends with an idle `FF` byte. Our earlier beta.6 FIFO method directly sent 55 bytes, without the priming and trailing idle bytes. The beta.9 FIFO method now sends a **single 60-byte fixed-length waveform** (`4+55+1`), within the CC1101's 64-byte FIFO, while preserving the same **one RQ/3220 Read-Data ID25**, 60-second cooldown, TX safety gates, and independent UART selection. This change may improve CC1101 TX startup but **does not prove RF delivery or that the R8810A will accept our virtual `18:` sender**.

In beta.9 the web interface calls this method **Reference FIFO (upstream bit-aligned, 60 RF bytes)**. Its JSON name remains `fifo_reference_primed_60`, with 60 bytes expected. It remains experimental; a local transmit completion does not prove the bridge received the message. Use the existing one-shot button (explicit confirmation); never enable automatic scans or polling on an unverified 433-MHz-marked module at 868.3 MHz. Decoder test coverage confirms precisely one frame after stripping the on-air UART framing. Passive RX debug from beta.6 remains available.

## Beta.6: Live RX debug switch and R8810A receive diagnostics

Open **Diagnostics → Passive radio receive diagnostics**. The **Enable detailed RX debug summaries** checkbox turns on/off rate-limited log summaries every 15 seconds **without saving settings or restarting**. OFF is the default after reboot. Even with debug OFF, lightweight receiver rejection and bridge counters are available from authenticated `GET /api/v1/radio/diagnostics`. The toggle uses `POST` to the same endpoint with `X-Gateway-Config: radio-debug` and `enabled=1` or `0`. It never transmits, changes RX filters or enables polling. Logs are exported through the existing debug download.

The diagnostic response separates controller `3EF0 RQ` requests **addressed to** configured `10:048994` from valid frames whose **sender is** the R8810A, including `3EF0` and `3220`. It reports checksum, malformed structure, invalid Manchester symbol, oversized, incomplete, idle timeout and formatting failures. **Rejected/invalid RF frames have no trustworthy sender attribution**, so reject counters cannot alone prove which device's reply was missed. Counters are since boot. The gateway continues to capture packets from every RF sender regardless of the configured virtual `18:` gateway ID.

CC1101 RSSI positive attenuation magnitude now uses **signed two's-complement** RSSI conversion rather than adding 256 to negative register values. This is a snapshot read after a frame, not a calibrated per-frame RF-power measurement. Existing passive heating operation and one-request safety gates remain unchanged. The compiler warning cleanup avoids deprecated `volatile++` expressions, fixes diagnostic printf argument types and bounds ISO UTC timestamp output.

# Arduino-framework RAMSES II gateway for ESP32-WROOM-32E

Arduino IDE prototype, receive-only by default; optional guarded read-only RF discovery for the 4 MB ESP32-WROOM-32E / NodeMCU-32, CC1101 868 MHz, Wi-Fi REST API and MQTT RX. **Not yet tested on physical hardware.**

## Download the standalone Arduino IDE ZIP

[RAMSES Arduino Gateway v0.4.0-beta.17 release](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.4.0-beta.17) — download the **RAMSES-Arduino-Gateway-v0.4.0-beta.17-ESP32-WROOM-32E.zip** asset (not the repository source-code ZIP). Unzip it, then open `RamsesGateway/RamsesGateway.ino` in Arduino IDE. This is a separately tagged **prerelease** for the radio gateway, not the Home Assistant integration.

## Beta.17: M5Stack M146 CC1101 RX stability diagnostics

**Release:** [RAMSES Arduino Gateway v0.4.0-beta.17](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.4.0-beta.17) — `RAMSES-Arduino-Gateway-v0.4.0-beta.17-ESP32-WROOM-32E.zip` (after CI/review/publishing).

This update packages the RX-health diagnostics merged in PR #47. It tracks the radio state across the observed stall after a single valid RAMSES message. The M5Stack M146 868 MHz M-BUS module uses **5V on pin 28**, unlike a bare CC1101 breakout (3.3V), and requires GPIO27/GDO2 connected as shown below.

## Passive CC1101 RX-state diagnostics

The authenticated `GET /api/v1/radio/rx-health` endpoint returns a protected
snapshot recorded by `radioTask`; HTTP does **not** access the CC1101 via SPI.
The report shows MARCSTATE (RX = 13), IOCFG2 (GDO2 asynchronous data = 13),
PKTCTRL0 (async format = 50), instantaneous GPIO27 level, UART-byte position,
and register readings before/after the existing 30-second RX re-entry.
Samples are collected at startup, approximately every five seconds while
the decoder is idle, and around re-entry. A bounded 5 ms settling check is
read-only (no extra RX or TX strobes). RX state/readback and sampled GPIO
voltage do not prove continuous successful RAMSES reception.

**M5Stack M146** (E07-900M10S 868 MHz) with your existing ESP32-WROOM-32E
uses the SPI/GDO GPIO mapping below, but its **M-BUS pin 28 requires 5V**:
pin 1 GND, pin 7 MOSI, pin 9 MISO, pin 11 SCK, switch-selected pin 8
CSN, pin 22 GDO0 and pin 2 GDO2. This differs from a bare CC1101 breakout,
which requires 3.3V. Disconnect USB and verify M-BUS positions before wiring.

## Wiring (verify actual labels on CC1101 breakout)

| CC1101 | NodeMCU-32 |
|---|---|
| VCC | 3V3 ONLY |
| GND | GND |
| SCK | GPIO18 / P18 |
| MISO / SO | GPIO19 / P19 |
| MOSI / SI | GPIO23 / P23 |
| CS / CSN | GPIO5 / P5 |
| GDO0 | GPIO26 / P26 |
| GDO2 | GPIO27 / P27 |

**For the photographed V2.0 module:** connector pin 1 GND = brown wire, pin 2 VCC = red wire to ESP32 3V3. **Disconnect USB before wiring; never connect the CC1101 to 5V.** Some breakout boards use 2.0 mm pitch, so verify jumper compatibility rather than forcing 2.54 mm Dupont sockets. Radio and antenna must support 868 MHz.

## Compile and upload from Arduino IDE

1. Install Arduino IDE 2.x and the Espressif board support URL: https://espressif.github.io/arduino-esp32/package_esp32_index.json
2. Install esp32 by Espressif Systems (Arduino core 3.x) in Boards Manager.
3. Install PubSubClient by Nick O'Leary (v2.8) in Library Manager.
4. Open RamsesGateway/RamsesGateway.ino. Select ESP32 Dev Module, 4MB flash, 240MHz, and COM5 (if unchanged).
5. Copy GatewaySecrets.example.h to GatewaySecrets.h in the sketch folder. Fill in Wi-Fi credentials and, optionally, an MQTT broker. The secrets file is gitignored. Without it, the sketch compiles but networking is disabled.
6. Optionally set `#define GW_HOSTNAME "evohome-gateway"` in your private `GatewaySecrets.h`; see hostname notes below.
7. Click Verify, then Upload, and open Serial Monitor at 115200 baud.
8. Open **Tools > Serial Monitor** at 115200, press **EN** once while the monitor is already open, and watch for BOOT / CC1101 / HEARTBEAT messages. Verify radio_detected in the web interface, then monitor valid_frames with your Evohome nearby.

## Experimental OpenTherm read-only discovery (v0.4.0 preview)

The main branch version 0.3.1 release stays RX-only. A new draft implementation
supports passive decoding of OpenTherm RP/3220, compatibility discovery with only
RQ/3220 Read-Data requests, persistent per-ID results, compatibility-derived
read-only polling, configurable CC1101 tuning and a web UI with a manual
**Start read-only test** action.

Refer to [READ_ONLY_OPENTHERM.md](READ_ONLY_OPENTHERM.md) for the complete
safety gates, private GatewaySecrets.h settings, compatibility classification,
API endpoints and known limitations. The RF transmit implementation is
UNTESTED on actual hardware and is disabled by default. Do not interpret
a successful Arduino build as validated communication with your R8810A.

**All 256 OpenTherm numeric IDs (0..255)** can be swept in an optional
explicit full scan, while the default test uses a curated read-candidate
list. No OpenTherm Write-Data or RAMSES W commands are available.
Some OpenTherm IDs are not readable, and a negative Read-Data answer does
not prove the underlying feature unsupported.

The CC1101 chip is multiband, but the module RF matching and antenna
are not automatically multiband. Evohome TX is hard-limited to 868.3 MHz.

## Configurable network hostname (new in v0.3.1)

In `GatewaySecrets.h`, set the hostname you want your router to display:

```cpp
#define GW_HOSTNAME "evohome-gateway"
```

This sets the ESP32 **DHCP Wi-Fi hostname** before connecting to your router and advertises a local mDNS HTTP service, generally browsable as **http://evohome-gateway.local/** if your computer, phone and network support mDNS. DHCP client-name display depends on your router. mDNS requires local multicast UDP/5353, may not cross VLANs, and is independent of the Home Assistant MQTT broker.

Use a single label of 1–63 ASCII letters, digits or hyphens, without a leading/trailing hyphen. Do not include spaces, underscores or `.local` in `GW_HOSTNAME`. Uppercase is normalized to lowercase. Empty, absent or invalid settings fall back to a unique `evohome-gateway-XXXXXX` suffix derived from the ESP32 hardware MAC.

**Upgrading from v0.3.0:** Your existing private `GatewaySecrets.h` does not contain this new setting. Add the `GW_HOSTNAME` line yourself when copying credentials into the v0.3.1 sketch folder, then upload. The gateway displays its active name and mDNS responder state in the browser dashboard and `/api/v1/status`.

mDNS responder startup is reported only after a successful Wi-Fi connection and retried after disconnects. It does not prove resolution from every client. Your device's MQTT identity and `RAMSES/GATEWAY/18:NNNNNN` topics stay unchanged.

## Browser-based diagnostics (new in v0.2.0)

The ESP32 now hosts a self-contained web debug dashboard. **No JTAG, GDB or external server required**: use the Arduino **Upload** button rather than Start Debugging. The red LED on a NodeMCU board is normally a power indicator, not a failure indicator.

- Visit **http://ESP32-IP/** (IP shown in Serial Monitor, your router's client list, or other LAN discovery).
- View CC1101 SPI detection, chip PARTNUM/VERSION, UART byte count, synchronization count, valid/invalid packets, Wi-Fi/MQTT, uptime, free heap and last reset reason.
- Diagnostic log refreshes every **2 seconds**, with severity selection and **Export log** (64 most recent entries).
- API: **GET /api/v1/debug/logs?since=0&level=3** and **GET /api/v1/debug/download**.
- A **heartbeat every 15 seconds** also appears in Serial Monitor after startup. A zero UART count means no bytes have reached the decoding task; it does not identify the root cause by itself.

### Optional independent Wi-Fi recovery AP

In `GatewaySecrets.h`, set a private WPA2 password with at least 8 characters:

```cpp
#define GW_RECOVERY_AP_PASSWORD "YOUR_UNIQUE_PRIVATE_PASSWORD"
```

After uploading, connect your laptop or phone to **RAMSES-Debug-XXXXXX** (unique suffix derived from the ESP32) and browse to **http://192.168.4.1/**. This access point operates alongside normal Wi-Fi and does not depend on an MQTT broker. Leave the password blank to disable it. **Do not use a publicly known default password.**

### Web security

The ESP32 web server is **HTTP only**, and should only be accessible from a trusted local network. Do not forward port 80 or expose it publicly. Optionally configure per-device HTTP Basic authentication in `GatewaySecrets.h`:

```cpp
#define GW_DEBUG_USER "admin"
#define GW_DEBUG_PASSWORD "CHOOSE_A_DIFFERENT_STRONG_PASSWORD"
```

HTTP Basic authentication does **not encrypt** the connection; use a trusted LAN/AP. The firmware never includes the configured Wi-Fi, MQTT or authentication passwords in its diagnostic output. If either username or password is blank, authentication is disabled (not suitable for untrusted networks).

### If the Serial Monitor is completely blank

Check the COM port selected in Tools > Port, set **115200 baud**, keep Serial Monitor open, and press the **EN** button. If you see neither ROM boot messages nor the 15-second heartbeat, the ESP32 firmware's execution and serial connection remain **unverified**; check with a simple Arduino Serial print test before assuming a CC1101 problem.

## API

- GET / returns a minimal device page.
- GET /api/v1/status returns connection/radio/counter/health information.
- GET /api/v1/packets returns up to 12 recent valid RAMSES II messages.

There are NO write, pairing or heating-control endpoints. The new experimental v0.4 preview provides a disabled-by-default, authenticated, strictly read-only RQ/3220 scan endpoint as described above. HTTP Basic authentication is optional; use a trusted LAN only and never port-forward it.

## MQTT Discovery for Home Assistant (new in v0.3.0)

With MQTT connected and Home Assistant MQTT Discovery enabled, the gateway automatically registers one device **RAMSES II Gateway**, containing eight diagnostic entities, even when the CC1101 is not connected:

- CC1101 radio (binary)
- Valid packets
- Rejected frames
- Dropped frames
- RF UART bytes
- RF syncs
- Free memory
- Uptime

The device's **availability** is tracked using the existing topic `RAMSES/GATEWAY/18:NNNNNN` with retained `online` and LWT `offline`; no separate MQTT Discovery connection sensor is necessary. The diagnostic values are reported every 15 seconds to `RAMSES/GATEWAY/18:NNNNNN/status`, retained. Home Assistant entity configuration is retained under `homeassistant/<component>/ramses_gateway_18_NNNNNN/<slug>/config`, and automatically republished on reconnect.

The identifier remains the same as in v0.2.0. These entities represent **gateway health**, not Evohome climate/zone devices. Native RF decoding and MQTT-based zone integration still require CC1101 hardware tests. The gateway remains receive-only by default; read-only RF diagnostic transmission must be explicitly configured.

To disable automatic discovery in a custom deployment, set `#define GW_MQTT_DISCOVERY 0` in `GatewaySecrets.h` before compiling (this does not delete already retained discovery configs from your broker).

**Verify on Home Assistant:** Settings → Devices & Services → MQTT → Devices. If not shown, listen to both `homeassistant/#` and `RAMSES/GATEWAY/#`; confirm the broker is the same and MQTT discovery is enabled.

## MQTT

RX topic: RAMSES/GATEWAY/18:NNNNNN/rx; payload is JSON with msg (HGI80-style) and ts (UTC ISO 8601), similar to IndaloTech RAMSES-ESP. Retained online/offline state: RAMSES/GATEWAY/18:NNNNNN. The ID is derived from ESP32 hardware ID; no MQTT TX subscriptions or commands are present.

Packets are not published to MQTT until network time has synchronized (NTP). Serial and REST remain available.

## Implementation and validation

CC1101 configuration follows the 868.3 MHz / 38.38 kbps radio parameters in IndaloTech/ramses_esp. CC1101 GDO2 connects to ESP32 UART1 at 38400 baud. The framing decoder handles RAMSES sync, Manchester nibble pairs and modulo-256 message checksums. RX is pinned to core 1; Arduino setup/loop may also be pinned to core 1 depending on build configuration, so scheduler/RF timing is NOT yet validated.

RSSI value is an approximation. Status exposes radio_verified=false until real radio reception is confirmed. ESP32 core compilation alone does not establish RF correctness.

This project is independent of the existing HA Evohome integration; the heating controller retains full local scheduling.

## Licensing and provenance

Receiver radio values and frame/Manc​hester decoding rules were adapted from Peter Price's IndaloTech/ramses_esp: https://github.com/IndaloTech/ramses_esp . Review upstream licensing and secure permission where required before redistributing derived firmware.

## Persistent configuration and timestamps (v0.4 beta)

Set `GW_TIMEZONE "Europe/Amsterdam"` in private `GatewaySecrets.h`
(default). The diagnostic log and exported text log display local dates,
including CET/CEST daylight-saving time automatically. The underlying
RAMSES/MQTT radio timestamps remain ISO-8601 UTC for interoperability.
Before SNTP synchronizes, event timestamps show `clock waiting for NTP`
rather than a fabricated calendar date; buffered earlier events receive
reconstructed wall-clock timestamps after synchronization.

The browser's **Gateway configuration** panel changes the settings *after*
flashing. Values are validated, committed as a sealed NVS record, and then
used as overrides to compiled `GatewaySecrets.h` defaults on every boot.
The browser cannot rewrite the compiled header. Wi-Fi, MQTT, DHCP hostname,
timezone, web login, recovery AP password, CC1101 frequency and read-only
OpenTherm diagnostic settings are supported. The physical CC1101 module's
declared hardware band remains a compile-time restriction. A new frequency
is applied by a controlled restart (not a live SPI reprogramming).

Passwords are never included in GET responses, and empty password inputs
keep the existing values. Save requires **existing** HTTP Basic login with
a strong password (12+ characters), a same-origin custom header, and a
trusted local network. HTTP is not TLS encrypted; do not publish port 80
on the Internet. After a Wi-Fi credential edit, the device waits up to
90 seconds for a successful connection; otherwise it restores the last
known-good NVS configuration and restarts. A configured WPA2 recovery
AP provides an additional fallback.

The RF radio may remain **physically disconnected**: the configuration,
timestamps, Wi-Fi and MQTT all continue to work. RF diagnostic-transmission
remains off by default and additionally requires an explicitly confirmed
radio, actual received bridge identity, 868.3 MHz matching hardware, and
valid RF addresses. No boiler write commands are available.



## Beta.5: A/B diagnostics for one-shot UART vs CC1101 TX FIFO

The manual **one-shot** test now has two radio transport choices. The original **async UART / GDO0** path is retained for baseline comparison; the **reference-style fixed 55-byte FIFO** path places the same encoded RAMSES II `3220` / OpenTherm Read-Data ID25 frame into the 64-byte TX FIFO and waits (with a hard 80ms limit) for local completion. Each button click sends **at most ONE request via ONE selected method**, with the existing 60-second cooldown and strong-auth/bridge-ID/RF frequency protections. The implementation does **not** automatically send both, and it does not enable general RF TX, scans, retries, or boiler writes.

Both methods explicitly program and verify the CC1101 PATABLE[0] to TI's nominal **868MHz -10dBm value (0x26)**. On a 433MHz-matched PCB actual RF output and legal compliance cannot be inferred from the PA register. The FIFO method keeps CC1101 GDO0 **high impedance** to avoid fighting the ESP32's existing UART TX wire, and polls MARCSTATE/TXBYTES over SPI rather than driving GDO0. FIFO mode uses bounded **fixed-length 44-byte packet mode** instead of the reference project's infinite-length streaming FIFO to prevent an unbounded RF stream on a stalled producer.

Diagnostics at `/api/v1/opentherm/one-shot` show selected method, local TX phase, PA programmed/read-back value, CC1101 MARCSTATE before/during/after, TX FIFO byte counters, bytes written, elapsed transmission time, RX restoration, and per-method A/B totals: attempted, locally completed, OpenTherm Read-Ack, reply timeout, other replies, local TX failures. **Local completion is not RF acknowledgement**. Counters reset on ESP32 restart. Compare actual Read-Ack results over separate individually authorized tests; timeout can reflect RF, address/protocol, or receiver issues. UART may produce a different waveform due to asynchronous packet format and serial framing; compare only one request at a time.

## Manual experimental one-request 3220 test on the existing 433M V2.0 PCB

This feature intentionally does **not** turn the existing 433M PCB into a certified 868 MHz transmitter. Passive RX at 868.3 MHz is working in the user's installation, but outbound power, sensitivity, emissions and interoperability have not been bench-tested.

On the ESP32 web **Settings** tab, retain **PCB: 433 MHz**, **868300 kHz**, **experimental cross-band RX: enabled**. Provide the *physically verified* R8810A decimal RF ID (e.g. packet source `10:048994` implies bridge ID `48994`) and a unique, unused 18: virtual RF source ID between 1 and 262142 (not reserved 730). **Do not reuse the MQTT gateway ID** (it may exceed RF address range). Leave **normal RF TX**, **automatic read polling** and **scan-all** OFF. Save and restart with strong HTTP authentication.

On the **Diagnostics** tab, expand **Experimental one-request read test**, confirm your radio identity and click the explicitly consented button. The gateway requires an RF-verified observed bridge and no observed collision of the chosen 18: source ID. A click sends **at most one** `RQ 3220` / OpenTherm **Read-Data ID25** (boiler flow temperature), with no retransmission, retries, scans, writes or setpoint overrides. Rate limit: 60 seconds after an attempt. Use `GET /api/v1/opentherm/one-shot` for queued, awaiting_reply, read_ack, data_invalid, unknown_data_id, timeout or tx_failed, including any flow-temperature value. The Web API requires Basic authentication and a request-specific custom header for POST.

This per-request diagnostic exception does **not** enable the existing bulk compatibility scanner, automatic RF requests, or a generic ability to transmit arbitrary RAMSES frames. On-air response and RF compliance are unverified until measured with the physical gateway and bridge. Do not assume a timeout proves unsupported OpenTherm ID25; it could be lost RF TX or RX.

## Browser Settings tab and experimental 868.3 MHz reception on 433M PCB

**Diagnostics** and **Settings** are now separate tabs on the ESP32 web interface. The Settings tab shows the fixed GPIO wiring and exposes the physical module marking (433/868/315/915 MHz), the receiver frequency in kHz, and an explicit **Allow experimental cross-band receive-only tuning** opt-in. After saving, the configuration persists in ESP32 NVS and takes effect after a controlled reboot. No firmware recompilation is needed for subsequent changes. Existing v1 NVS credentials and settings are migrated to v2.

For your pictured `433M V2.0` board, select **433 MHz**, frequency **868300 kHz**, tick experimental RX, and leave **RF read-request transmission OFF**. The silicon can tune to 868.3 MHz, but external RF matching and the supplied antenna are intended for 433 MHz: reception is **not assured**. The 433M module's vendor specifies operation in 387–464 MHz only, so this is a deliberately unverified reception experiment, not a supported product configuration. RF TX is disallowed whenever the declared PCB band is not 868 MHz and whenever experimental RX is enabled, even if the chip synthesizer tunes successfully.

The original GPIO wiring does not change: GDO0 GPIO26, GDO2 GPIO27, CS GPIO5, SCK GPIO18, MISO GPIO19, MOSI GPIO23, plus 3.3 V and ground. Software and CI checks cannot verify RF signal reception; you must confirm CC1101 chip detection and valid RAMSES frames on your hardware. 
