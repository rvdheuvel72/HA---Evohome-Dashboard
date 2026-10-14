// Copy to GatewaySecrets.h (gitignored); do not commit real credentials.
#pragma once
#define GW_WIFI_SSID "YOUR_WIFI_NAME"
#define GW_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Hostname advertised to DHCP and via multicast DNS (mDNS).
// Open http://evohome-gateway.local/ on devices that resolve .local addresses.
// One DNS label, 1-63 letters/digits/hyphens, no spaces, dots or underscores.
// To avoid collisions with multiple gateways, choose a different name per ESP32.
// Blank or missing GW_HOSTNAME generates evohome-gateway-XXXXXX from hardware MAC.
#define GW_HOSTNAME "evohome-gateway"
// IANA-labelled timezone; mapped to safe POSIX rules for ESP32 incl. CET/CEST.
#define GW_TIMEZONE "Europe/Amsterdam"
#define GW_MQTT_HOST "" // Optional; leave empty to disable MQTT
#define GW_MQTT_PORT 1883
#define GW_MQTT_USER ""
#define GW_MQTT_PASSWORD ""

// Strongly recommended: web diagnostics are accessible on your local network.
// HTTP Basic authentication has no transport encryption: trusted LAN only.
#define GW_DEBUG_USER "admin"
#define GW_DEBUG_PASSWORD "CHANGE_TO_A_STRONG_PRIVATE_PASSWORD"

// Optional independent WPA2 Wi-Fi recovery access point:
// After setting a password of at least 8 characters, connect to the
// RAMSES-Debug-XXXXXX Wi-Fi network and open http://192.168.4.1/.
// Leave empty to disable access-point mode.
#define GW_RECOVERY_AP_PASSWORD ""


// CC1101 tuning; these fields describe the PHYSICAL RF module and antenna.
 // The silicon supports multiple sub-GHz bands, but most modules are built
 // for ONE specific band. Do NOT set another band merely because the chip can.
 // Frequency in kHz. Evohome RAMSES II defaults to 868300 kHz.
#define GW_CC1101_MODULE_BAND 868
#define GW_CC1101_FREQUENCY_KHZ 868300

// Experimental read-only R8810A compatibility probing. Disabled by default.
// Enable ONLY after real RAMSES reception and identifying your bridge RF ID.
// RF transmission = sending RQ/3220 Read-Data packets, NEVER boiler writes.
// Exact bridge is 10:XXXXXX, e.g. 10:123456 -> enter integer 123456.
// Choose a legitimate unused 18:XXXXXX virtual RF gateway address (1..262142;
// 730 is reserved). This is NOT the MQTT device ID.
// RF transmission is always OFF on a first flash. To enable a diagnostic
// read-only scan later, use Gateway configuration in the authenticated web UI
// and confirm the module is physically connected and the bridge ID observed.
// A compile-time GW_OT_ENABLE_RF_TX define is intentionally NOT used.
#define GW_OT_BRIDGE_ID 0
#define GW_OT_SOURCE_ID 0
// 0: curated read candidates; 1: request every OpenTherm ID 0..255.
// Some IDs are write-only/parameterized: a Read-Data reply of Unknown-DataId
// does not prove the underlying boiler lacks the corresponding feature.
#define GW_OT_SCAN_ALL_256 0
// Disabled by default; after a completed scan, poll acknowledged, safe IDs
// one at a time with a minimum 60s gap if enabled.
#define GW_OT_AUTO_POLL 0
