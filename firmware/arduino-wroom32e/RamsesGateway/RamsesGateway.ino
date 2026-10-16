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
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <time.h>
#include <sys/time.h>
#include "Cc1101Radio.h"
#include "RamsesCodec.h"
#include "GatewayDebug.h"
#include "GatewayWebPage.h"
#include "GatewayDiscovery.h"
#include "GatewayHostname.h"
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
#ifndef GW_MQTT_DISCOVERY
#define GW_MQTT_DISCOVERY 1
#endif
#ifndef GW_HOSTNAME
#define GW_HOSTNAME "" // Legacy secrets: auto-generated unique hostname
#endif
// Frequency is chosen within the declared physical RF module band.
// The default CC1101 module and Evohome both use 868.3 MHz.
#ifndef GW_CC1101_FREQUENCY_KHZ
#define GW_CC1101_FREQUENCY_KHZ 868300
#endif
#ifndef GW_CC1101_MODULE_BAND
#define GW_CC1101_MODULE_BAND 868
#endif
// Experimental RF request transmission must be explicitly enabled,
// with RF IDs manually obtained from valid RAMSES traffic.
#ifndef GW_OT_BRIDGE_ID
#define GW_OT_BRIDGE_ID 0
#endif
#ifndef GW_OT_SOURCE_ID
#define GW_OT_SOURCE_ID 0
#endif
#ifndef GW_OT_SCAN_ALL_256
#define GW_OT_SCAN_ALL_256 0
#endif
#ifndef GW_OT_AUTO_POLL
#define GW_OT_AUTO_POLL 0
#endif
#ifndef GW_TIMEZONE
#define GW_TIMEZONE "Europe/Amsterdam"
#endif
#include "GatewayRuntimeConfig.h"
#include "OpenThermGateway.h"

namespace {
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t MQTT_RETRY_MS = 10000;
constexpr uint8_t RING_SIZE = 12;
constexpr uint32_t RX_IDLE_TIMEOUT_MS = 250;
constexpr uint32_t HEARTBEAT_MS = 15000;
constexpr uint8_t DEBUG_QUEUE_SIZE = 16;
constexpr char FW_VERSION[] = "0.4.0-beta.4";
constexpr uint32_t MDNS_RETRY_MS = 15000;
constexpr uint32_t MQTT_STATE_INTERVAL_MS = 15000;

struct PacketEvent {
  char hgi[256];
  uint32_t uptimeMs;
  uint8_t rssi;
  bool otValid=false;
  uint8_t otId=0;
  uint8_t otResult=0;
  uint16_t otRaw=0;
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
OpenThermGateway otGateway;
gw_config::Store configuration;
uint32_t restartDueMs=0;
uint32_t pendingBootMs=0;
QueueHandle_t rxQueue = nullptr;
QueueHandle_t debugQueue = nullptr;
GatewayDebug diagnosticLog;
bool recoveryApReady = false;
char recoveryApSsid[32] = {};
char gatewayHostname[64] = {};
bool mdnsReady = false;
uint32_t lastMdnsAttempt = 0;

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
char stateTopic[80] = {};
uint32_t lastMqttStateMs = 0;
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
  if (!configuration.current.webUser[0] || !configuration.current.webPassword[0]) return true;
  if (web.authenticate(configuration.current.webUser, configuration.current.webPassword)) return true;
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
  // GDO2 receives async radio data; GDO0 is UART TX into CC1101 during STX.
  // No RF TX is attempted unless explicit, constrained diagnostic gates pass.
  rfUart.begin(38400, SERIAL_8N1, Cc1101Radio::GDO2, Cc1101Radio::GDO0);
  radioReady = radio.begin(configuration.current.rfFrequencyKhz,
                            configuration.current.rfModuleBand,
                            configuration.current.rfExperimentalRx==1);
  if(radioReady)otGateway.begin();
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
        otGateway.noteDevice(message);
        const uint8_t strength = radio.rssiMagnitude();
        PacketEvent packet = {};
        if (ramses::format(message, strength, packet.hgi, sizeof(packet.hgi))) {
          packet.uptimeMs = millis();
          packet.rssi = strength;
          ot_readonly::Result otResult=ot_readonly::NOT_TESTED;
          uint8_t otId=0;
          uint16_t otRaw=0;
          if(otGateway.observe(message,otId,otResult,otRaw)){
            packet.otValid=true;
            packet.otId=otId;
            packet.otResult=otResult;
            packet.otRaw=otRaw;
          }
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
    otGateway.tick(radio,rfUart,decoder);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void httpOtProfile(){
  if(!authorized())return;
  otGateway.sendProfile(web);
}
void httpOtOneShot(){
  if(!authorized())return;
  if(web.method()==HTTP_GET){otGateway.sendOneShotStatus(web);return;}
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="one-shot" ||
     web.arg("confirm")!="experimental-single-read-25"){
    web.send(403,"application/json",
      "{\"error\":\"explicit_one_shot_confirmation_required\"}");
    return;
  }
  if(!configuration.current.webUser[0] ||
     strlen(configuration.current.webPassword)<12 ||
     strcmp(configuration.current.webPassword,
        "CHANGE_TO_A_STRONG_PRIVATE_PASSWORD")==0){
    web.send(403,"application/json","{\"error\":\"strong_web_auth_required\"}");
    return;
  }
  if(!radioReady || !otGateway.requestOneShot()){
    web.send(409,"application/json",
      "{\"error\":\"one_shot_not_ready_or_in_cooldown_check_status\"}");
    return;
  }
  web.send(202,"application/json",
    "{\"queued\":true,\"data_id\":25,\"attempt_limit\":1,\"mode\":\"experimental_read_only\"}");
}
void httpOtScan(){
  if(!authorized())return;
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="scan"){
    web.send(403,"application/json",
      "{\"error\":\"missing_same_origin_scan_header\"}");
    return;
  }
  // Never expose RF transmission on a gateway with anonymous HTTP
  // or the documented example/default placeholder password.
  if(!configuration.current.webUser[0] || strlen(configuration.current.webPassword)<12 ||
     strcmp(configuration.current.webPassword,"CHANGE_TO_A_STRONG_PRIVATE_PASSWORD")==0){
    web.send(403,"application/json","{\"error\":\"strong_web_auth_required\"}");
    return;
  }
  if(!radioReady || !otGateway.transmitEnabled()){
    web.send(409,"application/json","{\"error\":\"rf_tx_disabled_or_not_configured\"}");
    return;
  }
  if(!otGateway.requestScan()){
    web.send(409,"application/json","{\"error\":\"scan_already_active\"}");
    return;
  }
  web.send(202,"application/json","{\"status\":\"queued\",\"mode\":\"read_only\"}");
}

// Cross-site HTML forms cannot set this custom request header; browsers do
// not accept cross-origin credentialed writes without explicit CORS permission.
bool settingsWriteAllowed(){
  if (!configuration.current.webUser[0] ||
      strlen(configuration.current.webPassword)<12 ||
      strcmp(configuration.current.webPassword,
             "CHANGE_TO_A_STRONG_PRIVATE_PASSWORD")==0) {
    web.send(403,"application/json",
      "{\"error\":\"configure_strong_web_auth_in_GatewaySecrets_first\"}");
    return false;
  }
  if(!authorized())return false;
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="save") {
    web.send(403,"application/json","{\"error\":\"missing_same_origin_settings_header\"}");
    return false;
  }
  return true;
}
void httpConfigGet() {
  if(!authorized())return;
  const gw_config::Settings& cfg=configuration.current;
  String result="{";
  result+="\"hostname\":\""+escapeJSON(cfg.hostname)+"\",";
  result+="\"web_user\":\""+escapeJSON(cfg.webUser)+"\",";
  result+="\"wifi_ssid\":\""+escapeJSON(cfg.wifiSSID)+"\",";
  result+="\"mqtt_host\":\""+escapeJSON(cfg.mqttHost)+"\",";
  result+="\"mqtt_port\":"+String(cfg.mqttPort)+",";
  result+="\"mqtt_discovery\":"+(cfg.mqttDiscovery?String("true"):String("false"))+",";
  result+="\"mqtt_user\":\""+escapeJSON(cfg.mqttUser)+"\",";
  result+="\"timezone\":\""+escapeJSON(cfg.timezone)+"\",";
  result+="\"rf_frequency_khz\":"+String(cfg.rfFrequencyKhz)+",";
  result+="\"rf_module_band\":"+String(cfg.rfModuleBand)+",";
  result+="\"rf_experimental_rx\":"+(cfg.rfExperimentalRx?String("true"):String("false"))+",";
  result+="\"ot_bridge\":"+String(cfg.otBridge)+",";
  result+="\"ot_source\":"+String(cfg.otSource)+",";
  result+="\"ot_scan_all\":"+(cfg.otScanAll?String("true"):String("false"))+",";
  result+="\"ot_auto_poll\":"+(cfg.otAutoPoll?String("true"):String("false"))+",";
  result+="\"ot_tx_enabled\":"+(cfg.otTxEnabled?String("true"):String("false"))+",";
  result+="\"recovery_ap_configured\":"+(cfg.recoveryPassword[0]?String("true"):String("false"))+",";
  result+="\"web_auth_configured\":"+(cfg.webPassword[0]?String("true"):String("false"))+",";
  result+="\"wifi_password_configured\":"+(cfg.wifiPassword[0]?String("true"):String("false"))+",";
  result+="\"mqtt_password_configured\":"+(cfg.mqttPassword[0]?String("true"):String("false"))+",";
  result+="\"nvs_pending\":"+(configuration.pending?String("true"):String("false"))+"}";
  // Password values (WiFi, MQTT, web and recovery AP) are NEVER exposed.
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json",result);
}
bool readField(const char* key,char* dst,size_t cap,bool optional=false){
  if(!web.hasArg(key))return optional;
  const String value=web.arg(key);
  if(!gw_config::safeText(value,cap))return false;
  snprintf(dst,cap,"%s",value.c_str());
  return true;
}
void httpConfigSave(){
  if(!settingsWriteAllowed())return;
  if(configuration.pending){
    web.send(409,"application/json","{\"error\":\"previous_changes_waiting_for_wifi_confirmation\"}");
    return;
  }
  gw_config::Settings staged=configuration.current;
  bool ok=readField("hostname",staged.hostname,sizeof(staged.hostname)) &&
          readField("wifi_ssid",staged.wifiSSID,sizeof(staged.wifiSSID)) &&
          readField("mqtt_host",staged.mqttHost,sizeof(staged.mqttHost)) &&
          readField("mqtt_user",staged.mqttUser,sizeof(staged.mqttUser)) &&
          readField("timezone",staged.timezone,sizeof(staged.timezone)) &&
          readField("web_user",staged.webUser,sizeof(staged.webUser),true);
  if(!ok){
    web.send(400,"application/json","{\"error\":\"missing_or_invalid_text_field\"}");
    return;
  }
  // Empty password means retain existing secret. Clearing any secret requires
  // an explicit dedicated control, never an unnoticed empty form field.
  const struct {const char* key;char* value;size_t size;} passwords[]={
    {"wifi_password",staged.wifiPassword,sizeof(staged.wifiPassword)},
    {"mqtt_password",staged.mqttPassword,sizeof(staged.mqttPassword)},
    {"web_password",staged.webPassword,sizeof(staged.webPassword)},
    {"recovery_password",staged.recoveryPassword,sizeof(staged.recoveryPassword)}
  };
  for(const auto& field:passwords){
    if(web.hasArg(field.key) && web.arg(field.key).length()){
      if(!readField(field.key,field.value,field.size)){
        web.send(400,"application/json","{\"error\":\"invalid_password_length_or_characters\"}");
        return;
      }
    }
  }
  if(!web.hasArg("mqtt_port") || !web.hasArg("rf_frequency_khz") ||
     !web.hasArg("rf_module_band") ||
     !web.hasArg("ot_bridge") || !web.hasArg("ot_source")) {
    web.send(400,"application/json","{\"error\":\"required_numeric_field_missing\"}");
    return;
  }
  auto number=[](const String& text,unsigned long max,unsigned long& parsed)->bool {
    if(text.isEmpty() || text.length()>9)return false;
    for(size_t i=0;i<text.length();i++)
      if(text[i]<'0'||text[i]>'9')return false;
    parsed=strtoul(text.c_str(),nullptr,10);
    return parsed<=max;
  };
  unsigned long value=0;
  if(!number(web.arg("mqtt_port"),65535,value) || !value){
    web.send(400,"application/json","{\"error\":\"mqtt_port_must_be_1_to_65535\"}");return;
  }
  staged.mqttPort=static_cast<uint16_t>(value);
  if(!number(web.arg("rf_frequency_khz"),999999,value)){
    web.send(400,"application/json","{\"error\":\"invalid_rf_frequency\"}");return;
  }
  staged.rfFrequencyKhz=value;
  if(!number(web.arg("rf_module_band"),999,value) ||
     (value!=315 && value!=433 && value!=868 && value!=915)){
    web.send(400,"application/json","{\"error\":\"invalid_rf_board_marking\"}");return;
  }
  staged.rfModuleBand=static_cast<uint16_t>(value);
  staged.rfExperimentalRx=web.arg("rf_experimental_rx")=="1";
  if(!number(web.arg("ot_bridge"),262143,value)){
    web.send(400,"application/json","{\"error\":\"invalid_bridge_id\"}");return;
  }
  staged.otBridge=value;
  if(!number(web.arg("ot_source"),262143,value)){
    web.send(400,"application/json","{\"error\":\"invalid_source_id\"}");return;
  }
  staged.otSource=value;
  staged.otScanAll=web.arg("ot_scan_all")=="1";
  staged.otAutoPoll=web.arg("ot_auto_poll")=="1";
  staged.mqttDiscovery=web.arg("mqtt_discovery")=="1";
  staged.otTxEnabled=web.arg("ot_tx_enabled")=="1";
  if(staged.otTxEnabled && (!web.hasArg("rf_safety_confirm") ||
      web.arg("rf_safety_confirm")!="1")){
    web.send(400,"application/json","{\"error\":\"rf_read_request_safety_confirmation_required\"}");
    return;
  }
  if(web.arg("clear_wifi_password")=="1")staged.wifiPassword[0]=0;
  if(web.arg("clear_mqtt_password")=="1")staged.mqttPassword[0]=0;
  if(web.arg("clear_recovery_password")=="1")staged.recoveryPassword[0]=0;
  if(strlen(staged.webPassword)<12 || !staged.webUser[0] ||
     strcmp(staged.webPassword,"CHANGE_TO_A_STRONG_PRIVATE_PASSWORD")==0){
    web.send(400,"application/json","{\"error\":\"web_login_must_remain_secure\"}");
    return;
  }
  if(!gw_config::validate(staged,GW_CC1101_MODULE_BAND)){
    web.send(400,"application/json","{\"error\":\"invalid_settings_or_rf_band\"}");
    return;
  }
  if(!configuration.stage(staged,GW_CC1101_MODULE_BAND)){
    web.send(500,"application/json","{\"error\":\"nvs_save_failed\"}");
    return;
  }
  recordLog(GatewayDebug::INFO,"Authenticated settings update saved; restarting gateway");
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json","{\"saved\":true,\"restart_in_seconds\":3}");
  restartDueMs=millis()+3000;
}

void httpStatus() {
  if (!authorized()) return;
  char json[1550];
  const String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  const String apIp = recoveryApReady ? WiFi.softAPIP().toString() : "";
  snprintf(json, sizeof(json),
           "{\"version\":\"%s\",\"board\":\"ESP32-WROOM-32E\",\"mode\":\"%s\","
           "\"radio_detected\":%s,\"radio_verified\":%s,\"cc1101_part\":%u,\"cc1101_version\":%u,"
           "\"wifi_connected\":%s,\"mqtt_connected\":%s,\"time_synced\":%s,"
           "\"hostname\":\"%s\",\"mdns_active\":%s,\"cc1101_frequency_khz\":%lu,\"cc1101_module_band\":%u,\"ot_tx_enabled\":%s,"
           "\"valid_frames\":%lu,\"rejected_frames\":%lu,\"dropped_frames\":%lu,"
           "\"rf_uart_bytes\":%lu,\"rf_syncs\":%lu,\"radio_recalibrations\":%lu,"
           "\"radio_recalibration_failures\":%lu,"
           "\"last_frame_uptime_ms\":%lu,\"last_rssi\":%u,"
           "\"uptime_ms\":%lu,\"free_heap\":%lu,\"gateway_id\":\"%s\","
           "\"ip\":\"%s\",\"ap_ip\":\"%s\",\"ap_ssid\":\"%s\",\"reset_reason\":\"%s\"}",
           FW_VERSION,
           otGateway.transmitEnabled() ? "read_only_requests" : "receive_only",
           radioReady ? "true" : "false",
           validFrames ? "true" : "false", radio.part(), radio.version(),
           WiFi.status() == WL_CONNECTED ? "true" : "false",
           mqtt.connected() ? "true" : "false",
           timeSynchronized() ? "true" : "false",
           gatewayHostname, mdnsReady ? "true" : "false",
           static_cast<unsigned long>(radio.frequencyKhz()),
           static_cast<unsigned>(radio.moduleBand()),
           otGateway.transmitEnabled() ? "true" : "false",
           static_cast<unsigned long>(validFrames),
           static_cast<unsigned long>(rejectedFrames),
           static_cast<unsigned long>(droppedFrames),
           static_cast<unsigned long>(rfUartBytes),
           static_cast<unsigned long>(rfSyncs),
           static_cast<unsigned long>(radioRxRecalibrations),
           static_cast<unsigned long>(radioRxRecalibrationFailures),
           static_cast<unsigned long>(lastFrameMs), lastRssi,
           static_cast<unsigned long>(millis()),
           static_cast<unsigned long>(ESP.getFreeHeap()), deviceId,
           ip.c_str(), apIp.c_str(), recoveryApReady ? recoveryApSsid : "",
           resetReason());
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", json);
}

void httpPackets() {
  if (!authorized()) return;
  String json = "{\"packets\":[";
  for (uint8_t i = 0; i < recentCount; ++i) {
    if (i) json += ',';
    const uint8_t slot = (recentNext + RING_SIZE - recentCount + i) % RING_SIZE;
    json += "{\"ts\":\"";
    json += escapeJSON(recent[slot].timestamp);
    json += "\",\"msg\":\"";
    json += escapeJSON(recent[slot].message);
    json += "\"}";
  }
  json += "]}";
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", json);
}

// Wall clock for events before SNTP can be reconstructed after time sync.
bool eventLocalTime(uint32_t uptimeMs,char* out,size_t capacity) {
  if(!timeSynchronized() || !out || capacity<40)return false;
  const uint32_t elapsed=millis()-uptimeMs;
  const time_t instant=time(nullptr)-static_cast<time_t>(elapsed/1000u);
  tm local={};
  localtime_r(&instant,&local);
  return strftime(out,capacity,"%Y-%m-%d %H:%M:%S %Z (%z)",&local)>0;
}
void httpDebugLogs() {
  if (!authorized()) return;
  const uint32_t cursor = static_cast<uint32_t>(strtoul(web.arg("since").c_str(), nullptr, 10));
  const int maxValue = constrain(web.arg("level").toInt(), 0, 3);
  const auto maxLevel = static_cast<GatewayDebug::Level>(web.hasArg("level") ? maxValue : 2);
  web.sendHeader("Cache-Control", "no-store");
  web.setContentLength(CONTENT_LENGTH_UNKNOWN);
  web.send(200, "application/json", "");
  web.sendContent("{\"next\":" + String(diagnosticLog.newest()) + ",\"entries\":[");
  bool first = true;
  diagnosticLog.eachAfter(cursor, maxLevel, [&](const GatewayDebug::Entry& entry) {
    String item;
    item.reserve(240);
    if (!first) item += ',';
    first = false;
    item += "{\"seq\":";
    item += String(entry.sequence);
    item += ",\"uptime_ms\":";
    item += String(entry.uptimeMs);
    item += ",\"timestamp\":";
    char stamp[52]={};
    if(eventLocalTime(entry.uptimeMs,stamp,sizeof(stamp)))
      item += "\"" + escapeJSON(stamp) + "\"";
    else item += "null";
    item += ",\"level\":\"";
    item += GatewayDebug::levelName(entry.level);
    item += "\",\"message\":\"";
    item += escapeJSON(entry.message);
    item += "\"}";
    web.sendContent(item);
  });
  web.sendContent("]}");
  web.sendContent("");
}

void httpDebugDownload() {
  if (!authorized()) return;
  web.sendHeader("Cache-Control", "no-store");
  web.sendHeader("Content-Disposition", "attachment; filename=ramses-gateway-debug.txt");
  web.setContentLength(CONTENT_LENGTH_UNKNOWN);
  web.send(200, "text/plain", "");
  web.sendContent("RAMSES Arduino Gateway " + String(FW_VERSION) + " / " + resetReason() + "\n");
  diagnosticLog.eachAfter(0, GatewayDebug::TRACE, [&](const GatewayDebug::Entry& entry) {
    char stamp[52]={};
    String line="[";
    if(eventLocalTime(entry.uptimeMs,stamp,sizeof(stamp)))line+=stamp;
    else line+="+"+String(entry.uptimeMs)+"ms (not synchronized)";
    line+="] ";
    line += GatewayDebug::levelName(entry.level);
    line += " ";
    line += entry.message;
    line += "\n";
    web.sendContent(line);
  });
  web.sendContent("");
}

void httpIndex() {
  if (!authorized()) return;
  web.sendHeader("Cache-Control", "no-store");
  web.send_P(200, "text/html", GW_DEBUG_PAGE);
}

// Advertise HTTP diagnostics on local networks that support multicast DNS.
// mdnsReady reports local responder startup, not reachability from every client.
void serviceMdns() {
  if (WiFi.status() != WL_CONNECTED) {
    if (mdnsReady) {
      MDNS.end();
      mdnsReady = false;
      recordLog(GatewayDebug::WARN, "mDNS stopped on Wi-Fi disconnect");
    }
    return;
  }
  if (mdnsReady) return;
  const uint32_t now = millis();
  if (lastMdnsAttempt && now - lastMdnsAttempt < MDNS_RETRY_MS) return;
  lastMdnsAttempt = now;
  if (MDNS.begin(gatewayHostname)) {
    MDNS.addService("http", "tcp", 80);
    mdnsReady = true;
    char message[110];
    snprintf(message, sizeof(message), "mDNS ready: http://%s.local/", gatewayHostname);
    recordLog(GatewayDebug::INFO, message);
  } else {
    recordLog(GatewayDebug::WARN, "mDNS startup failed; retrying after 15s");
  }
}

void wifiConnect() {
  if (configuration.current.wifiSSID[0] == '\0') return;
  if (WiFi.status() == WL_CONNECTED) return;
  const uint32_t now = millis();
  if (lastWifiAttempt && now - lastWifiAttempt < WIFI_RETRY_MS) return;
  lastWifiAttempt = now;
  WiFi.disconnect(false);
  WiFi.begin(configuration.current.wifiSSID, configuration.current.wifiPassword);
  recordLog(GatewayDebug::INFO, "Wi-Fi connection attempt");
}

// MQTT Discovery creates a gateway device, not Evohome heating zone entities.
// Configuration and state retained so entities survive Home Assistant restarts.
bool publishDiscovery() {
  if (!mqtt.connected()) return false;
  bool success = true;
  for (size_t i = 0; i < gateway_discovery::ENTITY_COUNT; ++i) {
    char topic[144];
    char payload[1080];
    if (!gateway_discovery::buildConfig(i, deviceId, baseTopic, FW_VERSION,
                                         topic, sizeof(topic), payload, sizeof(payload))) {
      recordLog(GatewayDebug::ERROR, "MQTT Discovery payload overflow");
      return false;
    }
    if (!mqtt.publish(topic, payload, true)) {
      char msg[96];
      snprintf(msg, sizeof(msg), "MQTT Discovery publish failed, entity index=%u",
               static_cast<unsigned>(i));
      recordLog(GatewayDebug::WARN, msg);
      success = false;
    }
    mqtt.loop(); // Allow broker traffic between retained config messages.
    delay(2);
  }
  if (success) recordLog(GatewayDebug::INFO, "MQTT Discovery configurations published");
  return success;
}

bool publishGatewayState() {
  if (!mqtt.connected()) return false;
  char state[448];
  const int n = snprintf(state, sizeof(state),
    "{\"radio_detected\":%s,\"valid_frames\":%lu,\"rejected_frames\":%lu,"
    "\"dropped_frames\":%lu,\"rf_uart_bytes\":%lu,\"rf_syncs\":%lu,"
    "\"free_heap\":%lu,\"uptime_seconds\":%lu}",
    radioReady ? "true" : "false",
    static_cast<unsigned long>(validFrames),
    static_cast<unsigned long>(rejectedFrames),
    static_cast<unsigned long>(droppedFrames),
    static_cast<unsigned long>(rfUartBytes),
    static_cast<unsigned long>(rfSyncs),
    static_cast<unsigned long>(ESP.getFreeHeap()),
    static_cast<unsigned long>(millis()/1000));
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(state)) return false;
  return mqtt.publish(stateTopic, state, true);
}

void mqttConnect() {
  if (configuration.current.mqttHost[0] == '\0' || WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  const uint32_t now = millis();
  if (lastMqttAttempt && now - lastMqttAttempt < MQTT_RETRY_MS) return;
  lastMqttAttempt = now;
  bool ok;
  if (configuration.current.mqttUser[0] != '\0') {
    ok = mqtt.connect(deviceId, configuration.current.mqttUser, configuration.current.mqttPassword,
                      availabilityTopic, 1, true, "offline");
  } else {
    ok = mqtt.connect(deviceId, availabilityTopic, 1, true, "offline");
  }
  if (ok) {
    // Publish state and retained discovery before reporting availability.
    // A gateway with no CC1101 still registers as a device with radio=off.
    const bool stateOk = publishGatewayState();
    bool discoveryOk = true;
    if (configuration.current.mqttDiscovery) discoveryOk = publishDiscovery();
    if (!stateOk) recordLog(GatewayDebug::WARN, "Initial MQTT state publish failed");
    if (!discoveryOk) recordLog(GatewayDebug::WARN, "MQTT Discovery incomplete");
    mqtt.publish(availabilityTopic, "online", true);
    lastMqttStateMs = millis();
    recordLog(GatewayDebug::INFO, "MQTT connected");
    Serial.printf("# MQTT RX topic: %s\n", rxTopic);
  } else {
    char errorText[96];
    snprintf(errorText, sizeof(errorText), "MQTT connection failed, state=%d", mqtt.state());
    recordLog(GatewayDebug::WARN, errorText);
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
  if(event.otValid && mqtt.connected() && timeSynchronized()){
    char telemetry[240];
    float v=0;
    const bool isFixed=event.otResult==ot_readonly::READ_ACK &&
      ot_readonly::fixedPoint(event.otId,event.otRaw,v);
    if(isFixed)
      snprintf(telemetry,sizeof(telemetry),
        "{\"id\":%u,\"name\":\"%s\",\"status\":\"%s\",\"raw\":%u,\"value\":%.3f}",
        event.otId,ot_readonly::label(event.otId),
        ot_readonly::state(static_cast<ot_readonly::Result>(event.otResult)),
        event.otRaw,static_cast<double>(v));
    else
      snprintf(telemetry,sizeof(telemetry),
        "{\"id\":%u,\"name\":\"%s\",\"status\":\"%s\",\"raw\":%u,\"value\":null}",
        event.otId,ot_readonly::label(event.otId),
        ot_readonly::state(static_cast<ot_readonly::Result>(event.otResult)),
        event.otRaw);
    char topic[108];
    snprintf(topic,sizeof(topic),"%s/opentherm/event",baseTopic);
    mqtt.publish(topic,telemetry,false);
  }
  diagnosticLog.add(millis(), GatewayDebug::TRACE, event.hgi);

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
  Serial.println("\n# RAMSES II Arduino v0.4.0-beta.4 (read-only OpenTherm discovery)");
  recordLog(GatewayDebug::INFO, "Boot: RAMSES gateway v0.4.0-beta.4");
  char bootInfo[96];
  snprintf(bootInfo, sizeof(bootInfo), "Reset reason: %s", resetReason());
  recordLog(GatewayDebug::INFO, bootInfo);
  Serial.printf("# Core=%d MHz, flash=%lu bytes\n",
                getCpuFrequencyMhz(), static_cast<unsigned long>(ESP.getFlashChipSize()));
  rxQueue = xQueueCreate(12, sizeof(PacketEvent));
  debugQueue = xQueueCreate(DEBUG_QUEUE_SIZE, sizeof(DebugEvent));
  if (rxQueue == nullptr || debugQueue == nullptr) {
    recordLog(GatewayDebug::ERROR, "Could not allocate FreeRTOS queue");
    return;
  }
  const uint32_t serial = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFu);
  configuration.begin(GW_CC1101_MODULE_BAND);
  otGateway.configure(&configuration.current,configuration.current.rfModuleBand);
  const bool configuredHostnameValid = gateway_hostname::resolve(configuration.current.hostname, serial, gatewayHostname, sizeof(gatewayHostname));
  if (!configuredHostnameValid && configuration.current.hostname[0])
    recordLog(GatewayDebug::WARN, "GW_HOSTNAME invalid: using generated evohome-gateway-XXXXXX");
  char hostInfo[100];
  snprintf(hostInfo, sizeof(hostInfo), "Gateway hostname: %s", gatewayHostname);
  recordLog(GatewayDebug::INFO, hostInfo);
  unsigned id = serial % 999999u;
  if (id == 730u || id == 0) id += 1000u; // avoid reserved 18:000730 sentinel
  snprintf(deviceId, sizeof(deviceId), "18:%06u", id);
  snprintf(baseTopic, sizeof(baseTopic), "RAMSES/GATEWAY/%s", deviceId);
  snprintf(rxTopic, sizeof(rxTopic), "%s/rx", baseTopic);
  snprintf(availabilityTopic, sizeof(availabilityTopic), "%s", baseTopic);
  snprintf(stateTopic, sizeof(stateTopic), "%s/status", baseTopic);

  if (strlen(configuration.current.recoveryPassword) >= 8) {
    WiFi.mode(WIFI_AP_STA);
    snprintf(recoveryApSsid, sizeof(recoveryApSsid), "RAMSES-Debug-%06X", serial);
    recoveryApReady = WiFi.softAP(recoveryApSsid, configuration.current.recoveryPassword);
    if (recoveryApReady) {
      char apInfo[128];
      snprintf(apInfo, sizeof(apInfo), "Recovery AP active: %s, IP %s",
               recoveryApSsid, WiFi.softAPIP().toString().c_str());
      recordLog(GatewayDebug::INFO, apInfo);
    } else recordLog(GatewayDebug::WARN, "Recovery access point could not start");
  } else {
    WiFi.mode(WIFI_STA);
    recordLog(GatewayDebug::INFO, "Recovery AP disabled (set GW_RECOVERY_AP_PASSWORD)");
  }
  // Must set DHCP hostname AFTER WiFi.mode and BEFORE WiFi.begin.
  if (!WiFi.setHostname(gatewayHostname))
    recordLog(GatewayDebug::WARN, "Could not set DHCP Wi-Fi hostname");
  WiFi.setSleep(false);
  mqtt.setServer(configuration.current.mqttHost, configuration.current.mqttPort);
  mqtt.setBufferSize(1536); // Home Assistant Discovery configurations are larger than 512 bytes
  web.on("/", HTTP_GET, httpIndex);
  web.on("/api/v1/status", HTTP_GET, httpStatus);
  web.on("/api/v1/config", HTTP_GET, httpConfigGet);
  web.on("/api/v1/config", HTTP_POST, httpConfigSave);
  web.on("/api/v1/packets", HTTP_GET, httpPackets);
  web.on("/api/v1/opentherm/profile", HTTP_GET, httpOtProfile);
  web.on("/api/v1/opentherm/one-shot", HTTP_GET, httpOtOneShot);
  web.on("/api/v1/opentherm/one-shot", HTTP_POST, httpOtOneShot);
  web.on("/api/v1/opentherm/scan", HTTP_POST, httpOtScan);
  web.on("/api/v1/debug/logs", HTTP_GET, httpDebugLogs);
  web.on("/api/v1/debug/download", HTTP_GET, httpDebugDownload);
  web.onNotFound([]() {
    if (!authorized()) return;
    web.send(404, "application/json", "{\"error\":\"not_found\"}");
  });
  const char* configHeaders[]={"X-Gateway-Config"};
  web.collectHeaders(configHeaders,1);
  web.begin();
  recordLog(GatewayDebug::INFO, "HTTP diagnostics ready on port 80");
  // configTime() would overwrite TZ with UTC. configTzTime() preserves
  // full POSIX daylight-saving rules (CET/CEST for Europe/Amsterdam).
  configTzTime(gw_config::posixZone(configuration.current.timezone),
               "pool.ntp.org", "time.google.com");
  pendingBootMs=millis();
  wifiConnect();

  const BaseType_t created = xTaskCreatePinnedToCore(radioTask, "ramses-rx", 6144, nullptr, 3, nullptr, 1);
  if (created != pdPASS) recordLog(GatewayDebug::ERROR, "Could not start radio task");
}

void loop() {
  // Only loop() touches diagnosticLog; radio task sends events through debugQueue.
  DebugEvent debugEvent;
  unsigned flushed = 0;
  while (debugQueue && flushed < 12 && xQueueReceive(debugQueue, &debugEvent, 0) == pdTRUE) {
    recordLog(debugEvent.level, debugEvent.line);
    ++flushed;
  }
  wifiConnect();
  serviceMdns();
  mqttConnect();
  if (mqtt.connected()) {
    mqtt.loop();
    if (millis() - lastMqttStateMs >= MQTT_STATE_INTERVAL_MS) {
      lastMqttStateMs = millis();
      if (!publishGatewayState())
        recordLog(GatewayDebug::WARN, "MQTT state publish failed");
    }
  }
  web.handleClient();
  if(restartDueMs && static_cast<int32_t>(millis()-restartDueMs)>=0)ESP.restart();
  if(configuration.pending) {
    // Roll back staged credentials if Wi-Fi never reconnects after an edit.
    if(WiFi.status()==WL_CONNECTED && millis()-pendingBootMs>=20000) {
      if(configuration.confirm())recordLog(GatewayDebug::INFO,"Updated settings confirmed after Wi-Fi reconnection");
    } else if(millis()-pendingBootMs>=90000) {
      if(configuration.rollback(GW_CC1101_MODULE_BAND)) {
        recordLog(GatewayDebug::ERROR,"Settings Wi-Fi test failed, restoring previous configuration");
        delay(80);ESP.restart();
      }
    }
  }

  PacketEvent item;
  unsigned handled = 0;
  while (rxQueue && handled < 6 && xQueueReceive(rxQueue, &item, 0) == pdTRUE) {
    handlePacket(item);
    ++handled;
  }
  static uint32_t lastIpReport = 0;
  if (WiFi.status() == WL_CONNECTED && lastIpReport == 0) {
    recordLog(GatewayDebug::INFO, "Wi-Fi connected");
    Serial.printf("# Web API: http://%s/api/v1/status\n", WiFi.localIP().toString().c_str());
    if (mdnsReady) Serial.printf("# Gateway URL: http://%s.local/\n", gatewayHostname);
    lastIpReport = millis() ? millis() : 1;
  }
  if (WiFi.status() != WL_CONNECTED && lastIpReport != 0) {
    lastIpReport = 0;
    recordLog(GatewayDebug::WARN, "Wi-Fi disconnected");
  }
  static bool wasMqttConnected = false;
  const bool mqttNowConnected = mqtt.connected();
  if (wasMqttConnected && !mqttNowConnected) recordLog(GatewayDebug::WARN, "MQTT disconnected");
  wasMqttConnected = mqttNowConnected;
  static uint32_t lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();
    char heartbeat[130];
    snprintf(heartbeat, sizeof(heartbeat), "HEARTBEAT CC1101=%s UART=%lu sync=%lu valid=%lu rejected=%lu heap=%lu",
             radioReady ? "ok" : "missing",
             static_cast<unsigned long>(rfUartBytes), static_cast<unsigned long>(rfSyncs),
             static_cast<unsigned long>(validFrames), static_cast<unsigned long>(rejectedFrames),
             static_cast<unsigned long>(ESP.getFreeHeap()));
    recordLog(GatewayDebug::INFO, heartbeat);
  }
  delay(2);
}
