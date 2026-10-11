/*
 * Standalone RAMSES II CC1101 receiver for ESP32-WROOM-32E.
 * Arduino framework, receive-only prototype; Wi-Fi + REST + MQTT.
 * Based on protocol parameters documented by IndaloTech/ramses_esp.
 *
 * Arduino IDE: Board = ESP32 Dev Module; Flash = 4MB; Port = COM5.
 * Install PubSubClient by Nick O'Leary from Arduino Library Manager.
 * Create GatewaySecrets.h by copying GatewaySecrets.example.h.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <time.h>
#include <sys/time.h>
#include "Cc1101Radio.h"
#include "RamsesCodec.h"
#include "GatewayDebug.h"
#include "GatewayWebPage.h"
#include <esp_system.h>

#if __has_include("GatewaySecrets.h")
#include "GatewaySecrets.h"
#else
#define GW_WIFI_SSID ""
#define GW_WIFI_PASSWORD ""
#define GW_MQTT_HOST ""
#define GW_MQTT_PORT 1883
#define GW_MQTT_USER ""
#define GW_MQTT_PASSWORD ""
#endif

#ifndef GW_RECOVERY_AP_PASSWORD
#define GW_RECOVERY_AP_PASSWORD ""
#endif
#ifndef GW_DEBUG_USER
#define GW_DEBUG_USER ""
#endif
#ifndef GW_DEBUG_PASSWORD
#define GW_DEBUG_PASSWORD ""
#endif

namespace {
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t MQTT_RETRY_MS = 10000;
constexpr uint8_t RING_SIZE = 12;
constexpr uint32_t RX_IDLE_TIMEOUT_MS = 250;
constexpr uint32_t HEARTBEAT_MS = 15000;
constexpr uint8_t DEBUG_QUEUE_SIZE = 16;
constexpr char FW_VERSION[] = "0.2.0";

struct PacketEvent {
  char hgi[256];
  uint32_t uptimeMs;
  uint8_t rssi;
};
struct DebugEvent {
  char line[GatewayDebug::LINE_BYTES] = {};
  uint32_t ms = 0;
  GatewayDebug::Level level = GatewayDebug::INFO;
};

WebServer web(80);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
HardwareSerial rfUart(1);
Cc1101Radio radio;
QueueHandle_t rxQueue = nullptr;
QueueHandle_t debugQueue = nullptr;
GatewayDebug diagnosticLog;
bool recoveryApReady = false;
char recoveryApSsid[32] = {};

volatile bool radioReady = false;
volatile uint32_t validFrames = 0;
volatile uint32_t rejectedFrames = 0;
volatile uint32_t droppedFrames = 0;
volatile uint32_t lastFrameMs = 0;
volatile uint8_t lastRssi = 0;
volatile uint32_t rfUartBytes = 0;
volatile uint32_t rfSyncs = 0;
volatile uint32_t radioRxRecalibrations = 0;
volatile uint32_t radioRxRecalibrationFailures = 0;

char deviceId[12] = {};
char baseTopic[64] = {};
char rxTopic[72] = {};
char availabilityTopic[72] = {};
uint32_t lastWifiAttempt = 0;
uint32_t lastMqttAttempt = 0;

struct HistoryItem {
  char message[256];
  char timestamp[40];
  uint32_t ms = 0;
};
HistoryItem recent[RING_SIZE];
uint8_t recentNext = 0, recentCount = 0;

void recordLog(GatewayDebug::Level level, const char* message) {
  diagnosticLog.add(millis(), level, message);
  Serial.printf("# [%lu] %s: %s\n",
                static_cast<unsigned long>(millis()), GatewayDebug::levelName(level), message);
}

void radioLog(GatewayDebug::Level level, const char* message) {
  if (!debugQueue) return;
  DebugEvent event{};
  event.ms = millis();
  event.level = level;
  snprintf(event.line, sizeof(event.line), "%s", message);
  xQueueSend(debugQueue, &event, 0);
}

const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power_on";
    case ESP_RST_EXT: return "external_reset";
    case ESP_RST_SW: return "software_reset";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt_watchdog";
    case ESP_RST_TASK_WDT: return "task_watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "other";
  }
}

bool authorized() {
  if (!GW_DEBUG_USER[0] || !GW_DEBUG_PASSWORD[0]) return true;
  if (web.authenticate(GW_DEBUG_USER, GW_DEBUG_PASSWORD)) return true;
  web.requestAuthentication(BASIC_AUTH, "RAMSES Gateway Diagnostics");
  return false;
}

String escapeJSON(const char* input) {
  String out;
  out.reserve(strlen(input) + 12);
  for (const unsigned char* c = reinterpret_cast<const unsigned char*>(input); *c; ++c) {
    if (*c == '"' || *c == '\\') {
      out += '\\'; out += static_cast<char>(*c);
    } else if (*c == '\n') out += "\\n";
    else if (*c == '\r') out += "\\r";
    else if (*c < 32) out += '?';
    else out += static_cast<char>(*c);
  }
  return out;
}

bool timeSynchronized() {
  return time(nullptr) > 1600000000;
}

void makeTimestamp(char* out, size_t size) {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  struct tm utc = {};
  gmtime_r(&tv.tv_sec, &utc);
  snprintf(out, size, "%04d-%02d-%02dT%02d:%02d:%02d.%06ld+00:00",
           utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
           utc.tm_hour, utc.tm_min, utc.tm_sec, (long)tv.tv_usec);
}

void radioTask(void*) {
  ramses::FrameDecoder decoder;
  ramses::Message message;
  rfUart.setRxBufferSize(4096);
  // CC1101 GDO2 is RX serial data. No UART TX pin assigned.
  rfUart.begin(38400, SERIAL_8N1, Cc1101Radio::GDO2, -1);
  radioReady = radio.begin();
  char radioInfo[130];
  snprintf(radioInfo, sizeof(radioInfo), "CC1101 detected=%u PARTNUM=0x%02X VERSION=0x%02X",
           radioReady ? 1 : 0, radio.part(), radio.version());
  radioLog(radioReady ? GatewayDebug::INFO : GatewayDebug::ERROR, radioInfo);
  Serial.printf("# %s\n", radioInfo);
  if (!radioReady) {
    while (true) vTaskDelay(pdMS_TO_TICKS(2000)); // Safe: no RF access
  }

  uint32_t lastByteMs = millis();
  uint32_t lastCalibrationMs = millis();
  for (;;) {
    int processed = 0;
    while (rfUart.available() && processed < 512) {
      const uint8_t byteIn = static_cast<uint8_t>(rfUart.read());
      ++processed;
      ++rfUartBytes;
      lastByteMs = millis();
      const bool wasReceiving = decoder.receiving();
      const bool decoded = decoder.feed(byteIn, message);
      rfSyncs = decoder.syncCount();
      if (decoded) {
        const uint8_t strength = radio.rssiMagnitude();
        PacketEvent packet = {};
        if (ramses::format(message, strength, packet.hgi, sizeof(packet.hgi))) {
          packet.uptimeMs = millis();
          packet.rssi = strength;
          ++validFrames;
          lastFrameMs = packet.uptimeMs;
          lastRssi = strength;
          if (xQueueSend(rxQueue, &packet, 0) != pdTRUE) ++droppedFrames;
        } else {
          ++rejectedFrames;
        }
      } else if (wasReceiving && !decoder.receiving()) {
        ++rejectedFrames;
      }
    }

    const uint32_t now = millis();
    if (decoder.receiving() && (now - lastByteMs > RX_IDLE_TIMEOUT_MS)) {
      decoder.reset();
      ++rejectedFrames;
    }
    if (!decoder.receiving() && (now - lastCalibrationMs > 30000)) {
      // The upstream gateway also periodically re-enters RX to recalibrate.
      const bool ok = radio.enterReceive();
      if (ok) ++radioRxRecalibrations;
      else {
        ++radioRxRecalibrationFailures;
        radioLog(GatewayDebug::WARN, "CC1101 periodic RX recalibration failed");
      }
      lastCalibrationMs = now;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void httpStatus() {
  char json[480];
  snprintf(json, sizeof(json),
           "{\"board\":\"ESP32-WROOM-32E\",\"mode\":\"receive_only\","
           "\"radio_detected\":%s,\"radio_verified\":false,"
           "\"cc1101_part\":%u,\"cc1101_version\":%u,"
           "\"wifi_connected\":%s,\"mqtt_connected\":%s,\"time_synced\":%s,"
           "\"valid_frames\":%lu,\"rejected_frames\":%lu,\"dropped_frames\":%lu,"
           "\"last_frame_uptime_ms\":%lu,\"last_rssi\":%u,"
           "\"uptime_ms\":%lu,\"free_heap\":%lu,\"gateway_id\":\"%s\"}",
           radioReady ? "true" : "false",
           radio.part(), radio.version(),
           WiFi.status() == WL_CONNECTED ? "true" : "false",
           mqtt.connected() ? "true" : "false",
           timeSynchronized() ? "true" : "false",
           static_cast<unsigned long>(validFrames),
           static_cast<unsigned long>(rejectedFrames),
           static_cast<unsigned long>(droppedFrames),
           static_cast<unsigned long>(lastFrameMs), lastRssi,
           static_cast<unsigned long>(millis()),
           static_cast<unsigned long>(ESP.getFreeHeap()), deviceId);
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", json);
}

void httpPackets() {
  // HGI lines contain only ASCII digits, uppercase letters, colon,
  // dash and whitespace; preserve them as JSON strings.
  String json = "{\"packets\":[";
  for (uint8_t i = 0; i < recentCount; ++i) {
    if (i) json += ',';
    const uint8_t slot = (recentNext + RING_SIZE - recentCount + i) % RING_SIZE;
    json += "{\"ts\":\"";
    json += recent[slot].timestamp;
    json += "\",\"msg\":\"";
    json += recent[slot].message;
    json += "\"}";
  }
  json += "]}";
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", json);
}

void httpIndex() {
  web.send(200, "text/html",
           "<!doctype html><html><head><meta name='viewport' content='width=device-width'>"
           "<title>RAMSES II Gateway</title></head><body>"
           "<h1>RAMSES II Gateway</h1><p>ESP32-WROOM-32E / CC1101 / receive-only</p>"
           "<p><a href='/api/v1/status'>JSON status</a> | "
           "<a href='/api/v1/packets'>Recent packets</a></p>"
           "<p>Local-network prototype. No transmit endpoints.</p></body></html>");
}

void wifiConnect() {
  if (GW_WIFI_SSID[0] == '\0') return;
  if (WiFi.status() == WL_CONNECTED) return;
  const uint32_t now = millis();
  if (lastWifiAttempt && now - lastWifiAttempt < WIFI_RETRY_MS) return;
  lastWifiAttempt = now;
  WiFi.disconnect(false);
  WiFi.begin(GW_WIFI_SSID, GW_WIFI_PASSWORD);
  Serial.printf("# Wi-Fi connecting to %s\n", GW_WIFI_SSID);
}

void mqttConnect() {
  if (GW_MQTT_HOST[0] == '\0' || WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  const uint32_t now = millis();
  if (lastMqttAttempt && now - lastMqttAttempt < MQTT_RETRY_MS) return;
  lastMqttAttempt = now;
  bool ok;
  if (GW_MQTT_USER[0] != '\0') {
    ok = mqtt.connect(deviceId, GW_MQTT_USER, GW_MQTT_PASSWORD,
                      availabilityTopic, 1, true, "offline");
  } else {
    ok = mqtt.connect(deviceId, availabilityTopic, 1, true, "offline");
  }
  if (ok) {
    mqtt.publish(availabilityTopic, "online", true);
    Serial.printf("# MQTT connected, RX topic: %s\n", rxTopic);
  } else {
    Serial.printf("# MQTT connection failed state=%d\n", mqtt.state());
  }
}

void handlePacket(const PacketEvent& event) {
  char ts[40] = {};
  if (timeSynchronized()) makeTimestamp(ts, sizeof(ts));
  else strncpy(ts, "not-synchronized", sizeof(ts) - 1);

  HistoryItem& item = recent[recentNext];
  strncpy(item.message, event.hgi, sizeof(item.message) - 1);
  item.message[sizeof(item.message) - 1] = '\0';
  strncpy(item.timestamp, ts, sizeof(item.timestamp) - 1);
  item.timestamp[sizeof(item.timestamp) - 1] = '\0';
  item.ms = event.uptimeMs;
  recentNext = (recentNext + 1) % RING_SIZE;
  if (recentCount < RING_SIZE) ++recentCount;
  Serial.printf("# RX %s\n", event.hgi);

  // ramses_esp-compatible MQTT JSON. Refuse publishing stale/invalid timestamps.
  if (mqtt.connected() && timeSynchronized()) {
    char json[440];
    const int len = snprintf(json, sizeof(json),
                             "{\"msg\":\"%s\",\"ts\":\"%s\"}", event.hgi, ts);
    if (len > 0 && static_cast<size_t>(len) < sizeof(json))
      mqtt.publish(rxTopic, reinterpret_cast<const uint8_t*>(json),
                   static_cast<unsigned int>(len), false);
  }
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("\n# RAMSES II Arduino prototype 0.1.0 (RX ONLY)");
  Serial.printf("# Core=%d MHz, flash=%lu bytes\n",
                getCpuFrequencyMhz(), static_cast<unsigned long>(ESP.getFlashChipSize()));
  rxQueue = xQueueCreate(12, sizeof(PacketEvent));
  if (rxQueue == nullptr) {
    Serial.println("# ERROR: could not allocate RX queue");
    return;
  }
  const uint32_t serial = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFu);
  unsigned id = serial % 999999u;
  if (id == 730u || id == 0) id += 1000u; // avoid reserved 18:000730 sentinel
  snprintf(deviceId, sizeof(deviceId), "18:%06u", id);
  snprintf(baseTopic, sizeof(baseTopic), "RAMSES/GATEWAY/%s", deviceId);
  snprintf(rxTopic, sizeof(rxTopic), "%s/rx", baseTopic);
  snprintf(availabilityTopic, sizeof(availabilityTopic), "%s", baseTopic);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  mqtt.setServer(GW_MQTT_HOST, GW_MQTT_PORT);
  mqtt.setBufferSize(512); // RX JSON can exceed PubSubClient's standard buffer size
  web.on("/", HTTP_GET, httpIndex);
  web.on("/api/v1/status", HTTP_GET, httpStatus);
  web.on("/api/v1/packets", HTTP_GET, httpPackets);
  web.onNotFound([]() { web.send(404, "application/json", "{\"error\":\"not_found\"}"); });
  web.begin();
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  wifiConnect();

  xTaskCreatePinnedToCore(radioTask, "ramses-rx", 6144, nullptr, 3, nullptr, 1);
}

void loop() {
  wifiConnect();
  mqttConnect();
  if (mqtt.connected()) mqtt.loop();
  web.handleClient();

  PacketEvent item;
  unsigned handled = 0;
  while (rxQueue && handled < 6 && xQueueReceive(rxQueue, &item, 0) == pdTRUE) {
    handlePacket(item);
    ++handled;
  }
  static uint32_t lastIpReport = 0;
  if (WiFi.status() == WL_CONNECTED && lastIpReport == 0) {
    Serial.printf("# Web API: http://%s/api/v1/status\n", WiFi.localIP().toString().c_str());
    lastIpReport = millis() ? millis() : 1;
  }
  if (WiFi.status() != WL_CONNECTED) lastIpReport = 0;
  delay(2);
}
