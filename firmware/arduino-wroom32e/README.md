# Arduino-framework RAMSES II gateway for ESP32-WROOM-32E

Arduino IDE prototype, receive-only by default; optional guarded read-only RF discovery for the 4 MB ESP32-WROOM-32E / NodeMCU-32, CC1101 868 MHz, Wi-Fi REST API and MQTT RX. **Not yet tested on physical hardware.**

## Download the standalone Arduino IDE ZIP

[RAMSES Arduino Gateway v0.3.0 release](https://github.com/rvdheuvel72/HA---Evohome-Dashboard/releases/tag/ramses-arduino-v0.3.1) — download the **RAMSES-Arduino-Gateway-v0.3.1-ESP32-WROOM-32E.zip** asset (not the repository source-code ZIP). Unzip it, then open `RamsesGateway/RamsesGateway.ino` in Arduino IDE. This is a separately tagged **prerelease** for the radio gateway, not the Home Assistant integration.

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


## Browser Settings tab and experimental 868.3 MHz reception on 433M PCB

**Diagnostics** and **Settings** are now separate tabs on the ESP32 web interface. The Settings tab shows the fixed GPIO wiring and exposes the physical module marking (433/868/315/915 MHz), the receiver frequency in kHz, and an explicit **Allow experimental cross-band receive-only tuning** opt-in. After saving, the configuration persists in ESP32 NVS and takes effect after a controlled reboot. No firmware recompilation is needed for subsequent changes. Existing v1 NVS credentials and settings are migrated to v2.

For your pictured `433M V2.0` board, select **433 MHz**, frequency **868300 kHz**, tick experimental RX, and leave **RF read-request transmission OFF**. The silicon can tune to 868.3 MHz, but external RF matching and the supplied antenna are intended for 433 MHz: reception is **not assured**. The 433M module's vendor specifies operation in 387–464 MHz only, so this is a deliberately unverified reception experiment, not a supported product configuration. RF TX is disallowed whenever the declared PCB band is not 868 MHz and whenever experimental RX is enabled, even if the chip synthesizer tunes successfully.

The original GPIO wiring does not change: GDO0 GPIO26, GDO2 GPIO27, CS GPIO5, SCK GPIO18, MISO GPIO19, MOSI GPIO23, plus 3.3 V and ground. Software and CI checks cannot verify RF signal reception; you must confirm CC1101 chip detection and valid RAMSES frames on your hardware. 
