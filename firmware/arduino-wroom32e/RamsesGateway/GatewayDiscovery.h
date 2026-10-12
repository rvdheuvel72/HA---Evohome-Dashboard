// Home Assistant MQTT Discovery for the receive-only RAMSES gateway.
// Pure C++ header, without Arduino dependencies, so payloads can be unit-tested.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace gateway_discovery {

struct Entity {
  const char* component;
  const char* slug;
  const char* name;
  const char* property;
  const char* unit;
  const char* deviceClass;
  const char* stateClass;
  const char* icon;
};

// Stable names and unique IDs across gateway firmware updates.
constexpr Entity entities[] = {
  {"binary_sensor", "radio",    "CC1101 radio",   "radio_detected",  "",   "",         "",                 "mdi:radio-tower"},
  {"sensor",        "packets",  "Valid packets",  "valid_frames",    "",   "",         "total_increasing", "mdi:check-network"},
  {"sensor",        "rejected", "Rejected frames","rejected_frames", "",   "",         "total_increasing", "mdi:alert-circle-outline"},
  {"sensor",        "dropped",  "Dropped frames", "dropped_frames",  "",   "",         "total_increasing", "mdi:network-off-outline"},
  {"sensor",        "uart",     "RF UART bytes",  "rf_uart_bytes",   "B",  "",         "total_increasing", "mdi:serial-port"},
  {"sensor",        "sync",     "RF syncs",       "rf_syncs",        "",   "",         "total_increasing", "mdi:sync"},
  {"sensor",        "heap",     "Free memory",    "free_heap",       "B",  "data_size", "measurement",      "mdi:memory"},
  {"sensor",        "uptime",   "Uptime",         "uptime_seconds",  "s",  "duration",  "measurement",      "mdi:timer-outline"},
};
constexpr size_t ENTITY_COUNT = sizeof(entities) / sizeof(entities[0]);

inline bool validGatewayId(const char* id) {
  // Existing ID format from v0.1.0/v0.2.0, e.g. "18:447669".
  if (!id || strlen(id) != 9 || id[0] != '1' || id[1] != '8' || id[2] != ':') return false;
  for (size_t i=3; i<9; ++i) if (id[i] < '0' || id[i] > '9') return false;
  return true;
}

inline bool buildConfig(size_t index, const char* id, const char* base,
                        const char* version, char* topic, size_t topicSize,
                        char* payload, size_t payloadSize) {
  if (index >= ENTITY_COUNT || !validGatewayId(id) || !base || !version ||
      !topic || !payload || !topicSize || !payloadSize ||
      strlen(base) >= 100 || strlen(version) >= 32) return false;

  char key[40];
  const int nKey = snprintf(key, sizeof(key), "ramses_gateway_%c%c_%s", id[0], id[1], id+3);
  if (nKey < 0 || static_cast<size_t>(nKey) >= sizeof(key)) return false;
  const Entity& e = entities[index];
  const int nTopic = snprintf(topic, topicSize,
    "homeassistant/%s/%s/%s/config", e.component, key, e.slug);
  if (nTopic < 0 || static_cast<size_t>(nTopic) >= topicSize) return false;

  const int nPayload = snprintf(payload, payloadSize,
    "{\"name\":\"%s\",\"unique_id\":\"%s_%s\","
    "\"state_topic\":\"%s/status\","
    "\"value_template\":\"%s\","
    "\"availability_topic\":\"%s\","
    "\"payload_available\":\"online\",\"payload_not_available\":\"offline\","
    "\"entity_category\":\"diagnostic\",\"icon\":\"%s\","
    "\"device\":{\"identifiers\":[\"%s\"],"
    "\"name\":\"RAMSES II Gateway\","
    "\"manufacturer\":\"Community RAMSES\","
    "\"model\":\"ESP32-WROOM-32E with CC1101\","
    "\"sw_version\":\"%s\"}%s%s%s%s%s%s}",
    e.name, key, e.slug, base,
    // Binary state requires an explicit ON/OFF expression.
    index == 0 ? "{{ 'ON' if value_json.radio_detected else 'OFF' }}" :
                 /* placeholder substituted via static entity property templates below */ "",
    base, e.icon, key, version,
    e.unit[0] ? ",\"unit_of_measurement\":\"" : "",
    e.unit[0] ? e.unit : "",
    e.unit[0] ? "\"" : "",
    e.deviceClass[0] ? ",\"device_class\":\"" : "",
    e.deviceClass[0] ? e.deviceClass : "",
    e.deviceClass[0] ? "\"" : ""
  );
  // This initial formatter is used with an explicit full-template builder below.
  (void)nPayload;
  return false;
}
} // namespace gateway_discovery
