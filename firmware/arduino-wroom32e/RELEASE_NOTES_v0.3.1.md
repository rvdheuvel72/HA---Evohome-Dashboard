# RAMSES Arduino Gateway v0.3.1 — Custom DHCP hostname and mDNS

**Separate experimental Arduino IDE release** for ESP32-WROOM-32E (4 MB) + 868 MHz CC1101.

## What's new
- Configure `#define GW_HOSTNAME "evohome-gateway"` in your private `GatewaySecrets.h`.
- Configured name becomes the ESP32 DHCP Wi-Fi hostname, replacing the default `esp32-XXXXXX` shown by some routers.
- Multicast DNS advertises `http://evohome-gateway.local/` and the HTTP service on TCP/80 to compatible devices on the same local network.
- Name is validated (DNS single-label letters/digits/hyphens, 1–63 characters). Invalid, empty or missing settings fall back to unique `evohome-gateway-XXXXXX` derived from the MAC address.
- Browser diagnostics and `/api/v1/status` show hostname and local mDNS responder state.
- Existing MQTT Discovery sensors, topics, receiver behavior, browser logging and recovery AP continue unchanged.

## Upgrade from v0.3.0
1. Download `RAMSES-Arduino-Gateway-v0.3.1-ESP32-WROOM-32E.zip` and extract it.
2. Copy your existing private `GatewaySecrets.h` into the new `RamsesGateway` sketch folder.
3. **Add `#define GW_HOSTNAME "evohome-gateway"`** to that private file. Don't accidentally reuse the placeholder Wi-Fi credentials in the example.
4. Upload normally from Arduino IDE (ESP32 Dev Module; 4 MB flash; ESP32 Arduino core 3.x; PubSubClient 2.8).
5. Confirm Serial Monitor 115200 reports `Gateway hostname: evohome-gateway` and, once connected, `mDNS ready: http://evohome-gateway.local/`.
6. In your router's clients list, check its displayed host name; test the `.local` browser URL from a client on the same LAN. The numeric IP remains a fallback.

The MQTT device identity remains `18:NNNNNN`, avoiding duplicate Home Assistant devices. The DHCP lease IP may change depending on router configuration.

## Verification limitations
- CI compiles the Arduino sketch and tests hostname validation, receiver framing, web debug and Home Assistant MQTT Discovery.
- Physical hostname/DHCP/mDNS behaviour still needs testing on the user's real ESP32 and router. An mDNS startup log does not guarantee LAN name resolution on every network/VLAN.
- CC1101 radio still awaiting hardware testing; the firmware remains strictly receive-only.
- Local diagnostic HTTP is not encrypted; use a trusted LAN, not a forwarded Internet port.
- Release is independent of the Home Assistant integration release.
