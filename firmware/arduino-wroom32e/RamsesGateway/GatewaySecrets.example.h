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
