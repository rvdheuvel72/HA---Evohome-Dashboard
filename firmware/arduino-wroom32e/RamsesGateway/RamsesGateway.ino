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
#include "RamsesThermostat.h"
#include "RamsesRxHealth.h"
#include "RamsesSpiAudit.h"
#include "RamsesUartProbe.h"
#include "RamsesCodec.h"
#include "GatewayDebug.h"
#include "GatewayWebPage.h"
#include "GatewayWebScript.h"
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
#include "RamsesPacketCapture.h"
#include "OpenThermGateway.h"

namespace {
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t MQTT_RETRY_MS = 30000;
constexpr uint8_t RING_SIZE = 12;
constexpr uint32_t RX_IDLE_TIMEOUT_MS = 250;
constexpr uint32_t HEARTBEAT_MS = 15000;
constexpr uint32_t RADIO_RECOVERY_RETRY_MS = 5000;
constexpr uint32_t RX_HEALTH_SAMPLE_MS = 5000;
constexpr uint8_t DEBUG_QUEUE_SIZE = 32;
constexpr char FW_VERSION[] = "0.4.0-beta.37";
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
// Only radioTask touches CC1101 SPI; HTTP copies the protected report.
portMUX_TYPE rxHealthMux = portMUX_INITIALIZER_UNLOCKED;
ramses_rx_health::Report rxHealth;
portMUX_TYPE spiAuditMux = portMUX_INITIALIZER_UNLOCKED;
ramses_spi_audit::Snapshot spiAudit;
uint32_t spiAuditSamples=0;
portMUX_TYPE uartProbeMux = portMUX_INITIALIZER_UNLOCKED;
ramses_uart_probe::Snapshot uartProbe;
portMUX_TYPE thermostatMux = portMUX_INITIALIZER_UNLOCKED;
ramses_thermostat::Session thermostat;
bool thermostatForgetRequested=false; // guarded by thermostatMux; radioTask consumes
ramses_ids::SeenControllers thermostatIds;
ot_tx::Report thermostatTx;
char thermostatTxAction[24]="not_started";
char thermostatTopic[128]={};
volatile uint32_t validFrames = 0;
volatile uint32_t rejectedFrames = 0;
volatile uint32_t droppedFrames = 0;
volatile uint32_t lastFrameMs = 0;
volatile uint8_t lastRssi = 0;
volatile uint32_t rfUartBytes = 0;
volatile uint32_t rfSyncs = 0;
volatile uint32_t radioRxRecalibrations = 0;
volatile uint32_t radioRxRecalibrationFailures = 0;
// No RAMSES sync is not grounds for resetting a previously detected radio.
volatile uint32_t radioSilenceWarnings=0;
struct LoopLatency {
  uint32_t last_ms=0, max_ms=0, slow_calls=0;
  void observe(uint32_t elapsed) {
    last_ms=elapsed;
    if(elapsed>max_ms)max_ms=elapsed;
    if(elapsed>=500u)++slow_calls;
  }
};
LoopLatency httpLatency, mqttConnectLatency, mqttLoopLatency;
uint32_t httpServicedCount=0; // only HTTP handlers, never idle poll() calls
uint32_t httpUnservedSlowCalls=0, httpUnservedMaxMs=0;
uint32_t webAssetMaxMs=0, webAssetLastMs=0;
// Low-cost receive accounting always runs; optional 15s log summaries can be
// enabled or disabled instantly without writing NVS or restarting the ESP32.
portMUX_TYPE rxDiagnosticsMux = portMUX_INITIALIZER_UNLOCKED;
ramses_rx::Counters rxDiagnostics;
volatile bool rxDebugEnabled=false;
volatile uint32_t rxDebugEnabledAtMs=0;
// Capture records are written directly from radioTask BEFORE the 12-slot
// web/MQTT queue. Capture does not inherit that queue's drops or ring limit.
portMUX_TYPE packetCaptureMux=portMUX_INITIALIZER_UNLOCKED;
ramses_capture::Session packetCapture;


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

// Collect the earliest SPI observations before a browser can connect.
// Only radioTask touches CC1101; these are queued into serial/web debug log.
void logSpiStage(const char* phase,const ramses_spi_audit::Snapshot& s) {
  if(!s.sampled)return;
  char line[GatewayDebug::LINE_BYTES]={};
  if(!ramses_spi_audit::formatStartupLine(phase,s,line,sizeof(line))) {
    radioLog(GatewayDebug::ERROR,"SPI startup diagnostic line overflow");
    return;
  }
  radioLog(s.uniform_data()?GatewayDebug::WARN:GatewayDebug::INFO,line);
}
void logSpiBurst(const char* phase,const ramses_rx_bypass::Burst& s,
                 uint32_t expectedFreq) {
  if(!s.sampled)return;
  char line[GatewayDebug::LINE_BYTES]={};
  const int n=snprintf(line,sizeof(line),
    "SPI-%s t=%lu hdr=%02X io2=%02X pkt=%02X mdm=%02X freq=%02X%02X%02X ok=%u uni=%u",
    phase,static_cast<unsigned long>(s.uptime_ms),
    static_cast<unsigned>(s.chip_status),
    static_cast<unsigned>(s.regs[0]),static_cast<unsigned>(s.regs[8]),
    static_cast<unsigned>(s.regs[18]),
    static_cast<unsigned>(s.regs[13]),
    static_cast<unsigned>(s.regs[14]),
    static_cast<unsigned>(s.regs[15]),
    s.config_ok(expectedFreq)?1u:0u,s.uniform()?1u:0u);
  if(n<=0 || static_cast<size_t>(n)>=sizeof(line)){
    radioLog(GatewayDebug::ERROR,"SPI burst diagnostic log overflow");return;
  }
  radioLog(s.config_ok(expectedFreq)?GatewayDebug::INFO:GatewayDebug::WARN,line);
}
void logBootSpiTimeline() {
  logSpiStage("pre",radio.startupBeforeConfig());
  logSpiStage("cfg",radio.startupAfterConfig());
  // Readback after each existing receive-start operation. No extra RF
  // state transitions are introduced, only read-only SPI transactions.
  logSpiStage("idle",radio.startupRxStep(0));
  logSpiStage("iocfg",radio.startupRxStep(1));
  logSpiStage("packet",radio.startupRxStep(2));
  logSpiStage("flush",radio.startupRxStep(3));
  logSpiStage("srx-quiet",radio.startupRxStep(4));
  logSpiStage("srx",radio.startupAfterRx());
  // The radio driver also tried a quiet settle, an independent true
  // configuration burst and one bounded IDLE-to-RX recovery, if necessary.
  const auto& result=radio.rxBypassResult();
  if(result.attempted) {
    const uint32_t frequency=radio.startupAfterConfig().expected_freq_word;
    logSpiBurst("rxburst",result.rx_burst,frequency);
    logSpiStage("idle-check",result.idle_audit);
    logSpiBurst("idle-burst",result.idle_burst,frequency);
    logSpiStage("return",result.resumed_audit);
    char bypassLine[GatewayDebug::LINE_BYTES]={};
    snprintf(bypassLine,sizeof(bypassLine),
      "SPI-bypass sidle=%u reentry=%u fallback_srx=%u idle_spi_ok=%u return_spi_ok=%u",
      result.sidle_issued?1u:0u,result.reentry_issued?1u:0u,
      result.fallback_srx_issued?1u:0u,
      result.idle_audit.config_ok()?1u:0u,
      result.restored_valid_spi?1u:0u);
    radioLog(result.restored_valid_spi?GatewayDebug::INFO:GatewayDebug::WARN,bypassLine);
  }
  if(radio.startupSlowAudit1().sampled) {
    logSpiStage("slow1",radio.startupSlowAudit1());
    logSpiStage("slow2",radio.startupSlowAudit2());
    char clockLine[GatewayDebug::LINE_BYTES]={};
    snprintf(clockLine,sizeof(clockLine),
      "SPI-clock RX comparison 1MHz->125kHz accepted=%u selected_hz=%lu",
      radio.startupSlowRetained()?1u:0u,
      static_cast<unsigned long>(radio.spiClockHz()));
    radioLog(radio.startupSlowRetained()?GatewayDebug::INFO:GatewayDebug::WARN,clockLine);
  }
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
  // Every registered HTTP endpoint calls authorized() once at entry.
  // Count serviced requests even when Basic Auth rejects them, so last_ms
  // remains the duration of the last request instead of idle polls.
  ++httpServicedCount;
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
  if(!out || size<33)return; // Full ISO timestamp fits in 33 bytes including NUL.
  // strftime bounds each calendar field before adding the six microseconds.
  if(strftime(out,size,"%Y-%m-%dT%H:%M:%S",&utc)!=19) {
    out[0]='\0';
    return;
  }
  snprintf(out+19,size-19,".%06u+00:00",
           static_cast<unsigned>(tv.tv_usec%1000000L));
}

void appendCapture(ramses_capture::Kind kind,const char* message,
                   uint32_t uptime,uint8_t rssi) {
  uint64_t utcMs=0;
  if(timeSynchronized()) {
    struct timeval tv={};
    gettimeofday(&tv,nullptr);
    utcMs=static_cast<uint64_t>(tv.tv_sec)*1000u+
          static_cast<uint32_t>(tv.tv_usec/1000);
  }
  portENTER_CRITICAL(&packetCaptureMux);
  packetCapture.append(kind,message,uptime,rssi,utcMs);
  portEXIT_CRITICAL(&packetCaptureMux);
}

// GPIO-only UART1 TX -> UART2 RX. No CC1101 STX or RF packet transmission.
// The controller (01:) and the room thermostat (12:) use separate
// pairing sessions and the same single radio task. No concurrent RF TX.
bool sendThermostatFrame(const uint8_t* frame,size_t count,
                         ot_tx::Report& d,ramses::FrameDecoder& decoder) {
  d=ot_tx::Report{};
  uint8_t wire[128]={};
  const size_t size=ramses_read::encodeUART(frame,count,wire,sizeof(wire));
  if(!size || size>sizeof(wire)) {
    d.phase=ot_tx::TX_CONFIG_FAILED;return false;
  }
  d.expected_bytes=static_cast<uint16_t>(size);
  d.uart_min_expected_us=static_cast<uint32_t>(
    (static_cast<uint64_t>(size)*10000000ULL*9ULL)/(38400ULL*10ULL));
  const uint32_t started=micros();
  if(!radio.beginReadRequestTx(false,&d))return false;
  const uint32_t uartStarted=micros();
  const size_t written=rfUart.write(wire,size);
  rfUart.flush();
  d.uart_elapsed_us=micros()-uartStarted;
  d.marc_after_uart=radio.txMarcState();
  delayMicroseconds(300);
  const bool restored=radio.endReadRequestTx(&d);
  radio.verifyPostTxRx(d);
  while(rfUart.available())rfUart.read();
  decoder.reset();
  d.written_bytes=static_cast<uint16_t>(written);
  d.elapsed_us=micros()-started;
  d.phase=!restored?ot_tx::RX_RESTORE_FAILED:
    written!=size?ot_tx::UART_WRITE_FAILED:ot_tx::LOCAL_COMPLETE;
  return d.phase==ot_tx::LOCAL_COMPLETE;
}
void thermostatTick(ramses::FrameDecoder& decoder) {
  const uint32_t now=millis();
  ramses_thermostat::Session snap;
  portENTER_CRITICAL(&thermostatMux);
  if(thermostatForgetRequested) {
    thermostat.forget();
    thermostatTx=ot_tx::Report{};
    snprintf(thermostatTxAction,sizeof(thermostatTxAction),"locally_stopped");
    thermostatForgetRequested=false;
  }
  thermostat.tick(now);
  snap=thermostat;
  portEXIT_CRITICAL(&thermostatMux);
  const bool offers=snap.state==ramses_thermostat::OFFER_QUEUED;
  const bool confirms=snap.state==ramses_thermostat::CONFIRM_QUEUED;
  const bool reports=snap.reportDue(now);
  if(!offers && !confirms && !reports)return;
  // Prevent thermostat RF from overlapping any controller RF operations.
  if(!otGateway.manualRfIdle())return;
  if(!radioReady || !radio_frequency::evohomeTxFrequency(
       configuration.current.rfFrequencyKhz,configuration.current.rfModuleBand))
    return;
  uint8_t frame[64]={};size_t n=0;
  bool made=false;
  if(offers)
    made=ramses_thermostat::makeOffer(
      snap.thermostatId,snap.controllerId,frame,n);
  else if(confirms)
    made=ramses_thermostat::makeConfirmation(
      snap.thermostatId,snap.controllerId,snap.zone,frame,n);
  else if(reports)
    made=ramses_thermostat::makeTemperature(
      snap.thermostatId,snap.controllerId,snap.zone,snap.tempCenti,frame,n);
  ot_tx::Report d{};
  const bool sent=made && sendThermostatFrame(frame,n,d,decoder);
  if(!made)d.phase=ot_tx::TX_CONFIG_FAILED;
  portENTER_CRITICAL(&thermostatMux);
  thermostatTx=d;
  if(offers){
    thermostat.offered(sent,millis());
    snprintf(thermostatTxAction,sizeof(thermostatTxAction),"offer");
  } else if(confirms) {
    thermostat.confirmationSent(sent);
    snprintf(thermostatTxAction,sizeof(thermostatTxAction),"confirmation");
  } else {
    thermostat.reported(sent,millis(),snap.tempCenti);
    snprintf(thermostatTxAction,sizeof(thermostatTxAction),"temperature");
  }
  portEXIT_CRITICAL(&thermostatMux);
  char log[148];
  if(reports) {
    snprintf(log,sizeof(log),
      "Virtual 12 thermostat temperature %.2fC: TX=%s bytes=%u/%u (local only)",
      static_cast<double>(snap.tempCenti)/100.0,
      ot_tx::phaseName(d.phase),static_cast<unsigned>(d.written_bytes),
      static_cast<unsigned>(d.expected_bytes));
  } else {
    snprintf(log,sizeof(log),
      "Virtual 12 thermostat %s: TX=%s bytes=%u/%u (local only)",
      offers?"offer":"confirmation",
      ot_tx::phaseName(d.phase),static_cast<unsigned>(d.written_bytes),
      static_cast<unsigned>(d.expected_bytes));
  }
  radioLog(sent?GatewayDebug::INFO:GatewayDebug::WARN,log);
}

void radioTask(void*) {
  ramses::FrameDecoder decoder;
  ramses::Message message;
  ramses_rx_health::RecoveryMonitor rxRecovery;
  rfUart.setRxBufferSize(4096);
  // GDO2 receives async radio data; GDO0 is UART TX into CC1101 during STX.
  // No RF TX is attempted unless explicit, constrained diagnostic gates pass.
  rfUart.begin(38400, SERIAL_8N1, Cc1101Radio::GDO2, Cc1101Radio::GDO0);
  radioReady = radio.begin(configuration.current.rfFrequencyKhz,
                            configuration.current.rfModuleBand,
                            configuration.current.rfExperimentalRx==1);
  logBootSpiTimeline();
  if(radioReady) {
    otGateway.begin();
    const auto first=radio.readRxHealth(millis(),rfUartBytes);
    portENTER_CRITICAL(&rxHealthMux);
    rxHealth.observe(first);
    portEXIT_CRITICAL(&rxHealthMux);
    const auto audit=radio.readSpiAudit(millis());
    portENTER_CRITICAL(&spiAuditMux);
    spiAudit=audit;
    ++spiAuditSamples;
    portEXIT_CRITICAL(&spiAuditMux);
  }
  char radioInfo[130];
  snprintf(radioInfo, sizeof(radioInfo), "CC1101 initialized=%u PARTNUM=0x%02X VERSION=0x%02X stage=%s initial_MARC=%u RX_at_boot=%u",
           radioReady ? 1 : 0, radio.part(), radio.version(),
           radio.initStage(),static_cast<unsigned>(radio.startupMarcState()),
           radio.startupRxConfirmed()?1u:0u);
  radioLog(radioReady ? GatewayDebug::INFO : GatewayDebug::ERROR, radioInfo);
  Serial.printf("# %s\n", radioInfo);
  // A warm ESP32 reboot does not power-cycle the M5Stack radio. Retry a
  // bounded cold-compatible SRES sequence while leaving RF TX disabled.
  // Previously this branch waited forever until the user cut USB power.
  while (!radioReady) {
    vTaskDelay(pdMS_TO_TICKS(RADIO_RECOVERY_RETRY_MS));
    radioReady = radio.begin(configuration.current.rfFrequencyKhz,
                             configuration.current.rfModuleBand,
                             configuration.current.rfExperimentalRx==1);
    logBootSpiTimeline();
    if(radioReady) {
      otGateway.begin();
      const auto sample=radio.readRxHealth(millis(),rfUartBytes);
      portENTER_CRITICAL(&rxHealthMux);
      rxHealth.observe(sample);
      portEXIT_CRITICAL(&rxHealthMux);
      const auto audit=radio.readSpiAudit(millis());
      portENTER_CRITICAL(&spiAuditMux);
      spiAudit=audit;
      ++spiAuditSamples;
      portEXIT_CRITICAL(&spiAuditMux);
      radioLog(GatewayDebug::INFO,"CC1101 recovered after SPI/SRES retry; RX only");
    } else {
      radioLog(GatewayDebug::WARN,"CC1101 not initialized; retry in 5s (RX only)");
    }
  }

  uint32_t lastByteMs = millis();
  uint32_t lastCalibrationMs = millis();
  uint32_t lastHealthSampleMs = millis();
  uint32_t lastSpiAuditMs=millis();
  uint32_t lastRxDebugLogMs=millis();
  uint32_t nextSilentRecoveryMs=millis()+120000u;
  for (;;) {
    if(!radioReady){
      vTaskDelay(pdMS_TO_TICKS(RADIO_RECOVERY_RETRY_MS));
      radioReady=radio.begin(configuration.current.rfFrequencyKhz,
                              configuration.current.rfModuleBand,
                              configuration.current.rfExperimentalRx==1);
      logBootSpiTimeline();
      if(!radioReady)continue;
      radioLog(GatewayDebug::INFO,"CC1101 recovered after retry; RX verified");
    }
    int processed = 0;
    while (rfUart.available() && processed < 512) {
      const uint8_t byteIn = static_cast<uint8_t>(rfUart.read());
      ++processed;
      rfUartBytes = rfUartBytes + 1u;
      portENTER_CRITICAL(&uartProbeMux);
      uartProbe.observe(byteIn);
      portEXIT_CRITICAL(&uartProbeMux);
      lastByteMs = millis();
      const bool decoded = decoder.feed(byteIn, message);
      const ramses::RejectReason error=decoder.takeError();
      if(error!=ramses::NO_REJECTION) {
        char rejection[110];
        snprintf(rejection,sizeof(rejection),
                 "%s uart_byte=0x%02X sender=unknown",
                 ramses::rejectName(error),static_cast<unsigned>(byteIn));
        appendCapture(ramses_capture::REJECTION,rejection,millis(),0);
        rejectedFrames = rejectedFrames + 1u;
        portENTER_CRITICAL(&rxDiagnosticsMux);
        rxDiagnostics.rejection(error,millis());
        portEXIT_CRITICAL(&rxDiagnosticsMux);
      }
      rfSyncs = decoder.syncCount();
      if (decoded) {
        portENTER_CRITICAL(&rxDiagnosticsMux);
        rxDiagnostics.packet(message,configuration.current.otBridge,millis());
        portEXIT_CRITICAL(&rxDiagnosticsMux);
        otGateway.noteDevice(message);
        portENTER_CRITICAL(&thermostatMux);
        if((message.addrMask&1u) &&
           ramses_thermostat::type(message.addresses[0])==12)
          thermostatIds.note(ramses_thermostat::id(message.addresses[0]));
        thermostat.observe(message,millis());
        portEXIT_CRITICAL(&thermostatMux);
        const uint8_t strength = radio.rssiMagnitude();
        PacketEvent packet = {};
        if (ramses::format(message, strength, packet.hgi, sizeof(packet.hgi))) {
          packet.uptimeMs = millis();
          packet.rssi = strength;
          appendCapture(ramses_capture::PACKET,packet.hgi,
                        packet.uptimeMs,strength);
          ot_readonly::Result otResult=ot_readonly::NOT_TESTED;
          uint8_t otId=0;
          uint16_t otRaw=0;
          if(otGateway.observe(message,otId,otResult,otRaw)){
            packet.otValid=true;
            packet.otId=otId;
            packet.otResult=otResult;
            packet.otRaw=otRaw;
          }
          validFrames = validFrames + 1u;
          lastFrameMs = packet.uptimeMs;
          lastRssi = strength;
          if (xQueueSend(rxQueue, &packet, 0) != pdTRUE) droppedFrames = droppedFrames + 1u;
        } else {
          appendCapture(ramses_capture::REJECTION,
                        "format_failure decoded_frame_not_formatted",
                        millis(),0);
          rejectedFrames = rejectedFrames + 1u;
          portENTER_CRITICAL(&rxDiagnosticsMux);
          ++rxDiagnostics.formatFailure;
          portEXIT_CRITICAL(&rxDiagnosticsMux);
        }
      }
    }

    const uint32_t now = millis();
    // Quiet RAMSES traffic or a failing GDO2 wire is diagnostic evidence,
    // not permission to issue SRES. The old auto reset downgraded OK to
    // missing when no sync was found after two minutes.
    if(static_cast<int32_t>(now-nextSilentRecoveryMs)>=0 &&
       radioSilenceWarnings<2 && rfSyncs==0 && validFrames==0 &&
       rfUartBytes<256 && !decoder.receiving()) {
      radioSilenceWarnings=radioSilenceWarnings+1u;
      radioLog(GatewayDebug::WARN,
               "CC1101 no RAMSES sync; inspect RX health and GDO2; no SRES issued");
      nextSilentRecoveryMs=now+120000u;
    }
    if (decoder.receiving() && (now - lastByteMs > RX_IDLE_TIMEOUT_MS)) {
      decoder.abortIdleTimeout();
      const ramses::RejectReason reason=decoder.takeError();
      if(reason!=ramses::NO_REJECTION) {
        appendCapture(ramses_capture::REJECTION,
                      "idle_timeout incomplete_rf_candidate sender=unknown",
                      now,0);
        rejectedFrames = rejectedFrames + 1u;
        portENTER_CRITICAL(&rxDiagnosticsMux);
        rxDiagnostics.rejection(reason,now);
        portEXIT_CRITICAL(&rxDiagnosticsMux);
      }
    }
    // The beta.16 working driver re-entered RX every 30s regardless of
    // MARCSTATE. That can interrupt normal packet reception and does not
    // distinguish RF silence from radio failure. Read the state every 5s,
    // but strobe only after two independent *valid* IDLE/overflow readings.
    // Never SRES merely because no RAMSES packet has been heard.
    if (!decoder.receiving() && now-lastHealthSampleMs>=RX_HEALTH_SAMPLE_MS) {
      const auto before=radio.readRxHealth(now,rfUartBytes);
      const bool recover=rxRecovery.observe(before,false);
      if(recover && now-lastCalibrationMs>=30000u) {
        const bool ok=radio.enterReceive();
        if(ok)radioRxRecalibrations=radioRxRecalibrations+1u;
        else {
          radioRxRecalibrationFailures=radioRxRecalibrationFailures+1u;
          radioLog(GatewayDebug::WARN,"CC1101 confirmed non-RX; safe SRX re-entry failed");
        }
        const uint32_t started=micros();
        auto after=radio.readRxHealth(millis(),rfUartBytes);
        while(ok && !after.inRx() &&
              static_cast<uint32_t>(micros()-started)<5000u) {
          delayMicroseconds(100);
          after=radio.readRxHealth(millis(),rfUartBytes);
        }
        portENTER_CRITICAL(&rxHealthMux);
        rxHealth.recalibration(before,after,ok);
        portEXIT_CRITICAL(&rxHealthMux);
        rxRecovery.recovered();
        lastCalibrationMs=now;
      } else {
        portENTER_CRITICAL(&rxHealthMux);
        rxHealth.observe(before);
        portEXIT_CRITICAL(&rxHealthMux);
      }
      lastHealthSampleMs=now;
    }
    // Passive SPI register readback every 30s. No command strobes or writes.
    if(!decoder.receiving() && now-lastSpiAuditMs>=30000u) {
      const auto audit=radio.readSpiAudit(now);
      bool logRuntime=false;
      portENTER_CRITICAL(&spiAuditMux);
      // First runtime comparison (~30s), then only changed registers.
      logRuntime=(spiAuditSamples==1u) || audit.changedFrom(spiAudit);
      spiAudit=audit;
      ++spiAuditSamples;
      portEXIT_CRITICAL(&spiAuditMux);
      if(logRuntime)logSpiStage("runtime",audit);
      lastSpiAuditMs=now;
    }
    otGateway.tick(radio,rfUart,decoder);
    thermostatTick(decoder);
    if(rxDebugEnabled && now-lastRxDebugLogMs>=15000) {
      ramses_rx::Counters snapshot;
      portENTER_CRITICAL(&rxDiagnosticsMux);
      snapshot=rxDiagnostics;
      portEXIT_CRITICAL(&rxDiagnosticsMux);
      char line[GatewayDebug::LINE_BYTES]={};
      snprintf(line,sizeof(line),
        "RX debug: R8810A RQ=%lu received=%lu checksum=%lu symbol=%lu "
        "structure=%lu incomplete=%lu oversize=%lu idle=%lu",
        static_cast<unsigned long>(snapshot.requestsToBridge),
        static_cast<unsigned long>(snapshot.messagesFromBridge),
        static_cast<unsigned long>(snapshot.checksum),
        static_cast<unsigned long>(snapshot.invalidSymbol),
        static_cast<unsigned long>(snapshot.malformed),
        static_cast<unsigned long>(snapshot.incomplete),
        static_cast<unsigned long>(snapshot.oversized),
        static_cast<unsigned long>(snapshot.idleTimeout));
      radioLog(GatewayDebug::INFO,line);
      lastRxDebugLogMs=now;
    }
    if(!rxDebugEnabled)lastRxDebugLogMs=now;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void httpOtProfile(){
  if(!authorized())return;
  otGateway.sendProfile(web);
}
// Local GPIO-only diagnostic. This endpoint never configures CC1101 TX.
// A room thermostat is a separate type-12 device, paired with the REAL
// Evohome 01: controller, not with the R8810A OpenTherm bridge.
bool strongThermostatAuth() {
  if(!configuration.current.webUser[0] ||
     strlen(configuration.current.webPassword)<12 ||
     strcmp(configuration.current.webPassword,
            "CHANGE_TO_A_STRONG_PRIVATE_PASSWORD")==0) {
    web.send(403,"application/json","{\"error\":\"strong_web_auth_required\"}");
    return false;
  }
  return authorized();
}
bool thermostatTempValue(const String& text,int32_t& result){
  if(text.length()<1 || text.length()>5)return false;
  size_t i=text[0]=='-'?1:0;
  if(i==text.length())return false;
  for(;i<text.length();i++)if(text[i]<'0'||text[i]>'9')return false;
  const long parsed=text.toInt();
  if(parsed<-4000 || parsed>8500)return false;
  result=static_cast<int32_t>(parsed);
  return true;
}
void httpThermostatStatus(){
  if(!authorized())return;
  ramses_thermostat::Session snap;ot_tx::Report tx;char txAction[24];
  bool eligible=false;unsigned seen=0;
  portENTER_CRITICAL(&thermostatMux);
  snap=thermostat;tx=thermostatTx;
  snprintf(txAction,sizeof(txAction),"%s",thermostatTxAction);
  eligible=!thermostatIds.saturated();seen=thermostatIds.count;
  portEXIT_CRITICAL(&thermostatMux);
  const uint32_t controller=otGateway.observedControllerId();
  const bool safe=radioReady && controller>0 && otGateway.bridgeObserved() &&
    otGateway.manualRfIdle() && eligible &&
    radio_frequency::evohomeTxFrequency(configuration.current.rfFrequencyKhz,
                                        configuration.current.rfModuleBand);
  const uint32_t now=millis();
  char data[1100];
  const int len=snprintf(data,sizeof(data),
    "{\"role\":\"12_virtual_room_thermostat\",\"state\":\"%s\","
    "\"ready\":%s,\"controller_id\":%lu,\"thermostat_id\":%lu,"
    "\"zone\":%d,\"offer_accepted\":%s,\"confirmation_sent\":%s,"
    "\"cooldown_ms\":%lu,\"id_conflict\":%s,\"ids_seen\":%u,"
    "\"temp_centi\":%ld,\"temperature_fresh\":%s,\"report_due\":%s,"
    "\"report_pending\":%s,\"next_report_in_ms\":%lu,"
    "\"last_report_attempted\":%s,\"last_report_centi\":%ld,"
    "\"last_report_local_complete\":%s,"
    "\"last_tx_action\":\"%s\",\"tx_phase\":\"%s\","
    "\"tx_local_verified\":%s,\"tx_bytes\":%u,\"tx_expected_bytes\":%u,"
    "\"mqtt_temperature_topic\":\"%s\","
    "\"note\":\"Controller acceptance observed on RX; confirmation and 30C9 TX are local only\"}",
    ramses_thermostat::stateName(snap.state),
    (safe&&!snap.busy()&&snap.cooldown(now)==0)?"true":"false",
    static_cast<unsigned long>(controller),
    static_cast<unsigned long>(snap.thermostatId),
    snap.zone==0xff?-1:static_cast<int>(snap.zone),
    snap.accepted?"true":"false",snap.confirmed?"true":"false",
    static_cast<unsigned long>(snap.cooldown(now)),
    snap.collision?"true":"false",seen,
    static_cast<long>(snap.tempCenti),
    snap.temperatureFresh(now)?"true":"false",
    snap.reportDue(now)?"true":"false",
    snap.reportPending(now)?"true":"false",
    static_cast<unsigned long>(snap.nextReportInMs(now)),
    snap.hasReported?"true":"false",
    static_cast<long>(snap.lastReportCenti),
    snap.lastReportLocalComplete?"true":"false",
    txAction,ot_tx::phaseName(tx.phase),
    ot_tx::localVerified(tx)?"true":"false",
    static_cast<unsigned>(tx.written_bytes),
    static_cast<unsigned>(tx.expected_bytes),
    thermostatTopic);
  if(len<0 || static_cast<size_t>(len)>=sizeof(data)){
    web.send(500,"application/json","{\"error\":\"thermostat_status_overflow\"}");
    return;
  }
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json",data);
}
void httpThermostatPairing(){
  if(!strongThermostatAuth())return;
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="thermostat-pairing" ||
     web.arg("confirm")!="manual-12-thermostat-bind"){
    web.send(403,"application/json","{\"error\":\"manual_binding_confirmation_required\"}");
    return;
  }
  const String text=web.arg("thermostat_id");
  if(text.isEmpty() || text.length()>6){
    web.send(400,"application/json","{\"error\":\"invalid_thermostat_id\"}");return;
  }
  for(size_t i=0;i<text.length();i++)
    if(text[i]<'0'||text[i]>'9'){
      web.send(400,"application/json","{\"error\":\"invalid_thermostat_id\"}");return;
    }
  const uint32_t id=static_cast<uint32_t>(text.toInt());
  const uint32_t controller=otGateway.observedControllerId();
  if(!radioReady || !controller || !otGateway.bridgeObserved() ||
     !otGateway.manualRfIdle() ||
     !radio_frequency::evohomeTxFrequency(configuration.current.rfFrequencyKhz,
                                           configuration.current.rfModuleBand)){
    web.send(409,"application/json","{\"error\":\"controller_or_radio_not_ready\"}");
    return;
  }
  bool accepted=false;
  portENTER_CRITICAL(&thermostatMux);
  if(!thermostatForgetRequested && thermostatIds.eligible(id) && !thermostat.busy())
    accepted=thermostat.request(millis(),id,controller);
  portEXIT_CRITICAL(&thermostatMux);
  if(!accepted) {
    web.send(409,"application/json","{\"error\":\"invalid_id_collision_or_cooldown\"}");
    return;
  }
  recordLog(GatewayDebug::WARN,"Manual type-12 thermostat 1FC9 offer queued to observed Evohome controller");
  web.send(202,"application/json","{\"queued\":true,\"device_type\":12,\"automatic_rf\":false}");
}
void httpThermostatForget(){
  if(!strongThermostatAuth())return;
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="thermostat-forget" ||
     web.arg("confirm")!="stop-and-forget-local-12"){
    web.send(403,"application/json","{\"error\":\"explicit_local_stop_confirmation_required\"}");
    return;
  }
  const String idArg=web.arg("thermostat_id");
  if(idArg.isEmpty() || idArg.length()>6) {
    web.send(400,"application/json","{\"error\":\"thermostat_id_required\"}");
    return;
  }
  for(size_t i=0;i<idArg.length();i++)if(idArg[i]<'0'||idArg[i]>'9'){
    web.send(400,"application/json","{\"error\":\"invalid_thermostat_id\"}");return;
  }
  const uint32_t expectedId=static_cast<uint32_t>(idArg.toInt());
  bool queued=false;
  portENTER_CRITICAL(&thermostatMux);
  if(expectedId>0 && thermostat.thermostatId==expectedId &&
     thermostat.state!=ramses_thermostat::IDLE &&
     !thermostatForgetRequested) {
    // Remove pending measurements immediately; the radio task consumes
    // the local stop request before starting another thermostat frame.
    thermostat.hasTemp=false;
    thermostatForgetRequested=true;
    queued=true;
  }
  portEXIT_CRITICAL(&thermostatMux);
  if(!queued) {
    web.send(409,"application/json","{\"error\":\"virtual_thermostat_not_active_or_id_mismatch\"}");
    return;
  }
  recordLog(GatewayDebug::WARN,"Virtual 12 thermostat local stop queued (no RF unbind)");
  web.send(202,"application/json",
    "{\"queued\":true,\"scope\":\"local_only\",\"remote_evohome_binding_removed\":false}");
}
void httpVirtualControllerForget(){
  if(!strongThermostatAuth())return;
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="virtual-controller-forget" ||
     web.arg("confirm")!="stop-and-forget-local-01"){
    web.send(403,"application/json","{\"error\":\"explicit_local_stop_confirmation_required\"}");
    return;
  }
  const String idArg=web.arg("virtual_controller_id");
  if(idArg.isEmpty() || idArg.length()>6){
    web.send(400,"application/json","{\"error\":\"virtual_controller_id_required\"}");return;
  }
  for(size_t i=0;i<idArg.length();i++)if(idArg[i]<'0'||idArg[i]>'9'){
    web.send(400,"application/json","{\"error\":\"invalid_virtual_controller_id\"}");return;
  }
  if(!otGateway.requestForgetBinding(static_cast<uint32_t>(idArg.toInt()))){
    web.send(409,"application/json",
      "{\"error\":\"virtual_controller_not_active_or_id_mismatch\"}");
    return;
  }
  recordLog(GatewayDebug::WARN,"Virtual 01 controller local stop queued (R8810A binding unchanged)");
  web.send(202,"application/json",
    "{\"queued\":true,\"scope\":\"local_only\",\"remote_r8810a_binding_removed\":false}");
}

void httpThermostatTemperature(){
  if(!strongThermostatAuth())return;
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="thermostat-temperature" ||
     web.arg("confirm")!="set-sensor-temperature"){
    web.send(403,"application/json","{\"error\":\"temperature_confirmation_required\"}");return;
  }
  int32_t centi=0;
  if(!thermostatTempValue(web.arg("temp_centi"),centi)){
    web.send(400,"application/json","{\"error\":\"invalid_centi_celsius_range\"}");return;
  }
  bool accepted=false,pending=false;
  uint32_t nextMs=0;
  portENTER_CRITICAL(&thermostatMux);
  if(thermostat.state==ramses_thermostat::PAIRED) {
    const uint32_t now=millis();
    accepted=thermostat.setTemperature(centi,now);
    pending=thermostat.reportPending(now);
    nextMs=thermostat.nextReportInMs(now);
  }
  portEXIT_CRITICAL(&thermostatMux);
  if(!accepted){
    web.send(409,"application/json","{\"error\":\"thermostat_not_controller_accepted\"}");return;
  }
  char result[158];
  snprintf(result,sizeof(result),
    "{\"accepted\":true,\"unit\":\"centi_celsius\",\"report_pending\":%s,"
    "\"next_report_in_ms\":%lu}",
    pending?"true":"false",static_cast<unsigned long>(nextMs));
  web.send(200,"application/json",result);
}
void onMqttMessage(char* topic,uint8_t* payload,unsigned length){
  if(!topic || strcmp(topic,thermostatTopic)!=0 ||
     length<12 || length>18 || !payload)return;
  // A broker replay on reconnect is indistinguishable from a live message
  // in PubSubClient. Require a source timestamp and NEVER refresh its age
  // to the time of receipt. Stale samples fail closed when NTP is unavailable.
  const time_t epoch=time(nullptr);
  if(epoch<1600000000)return;
  int32_t centi=0;uint32_t ageMs=0;
  if(!ramses_thermostat::mqttStampedTemperature(
        reinterpret_cast<const char*>(payload),length,
        static_cast<uint32_t>(epoch),centi,ageMs))return;
  const uint32_t sampledAt=millis()-ageMs;
  portENTER_CRITICAL(&thermostatMux);
  if(thermostat.state==ramses_thermostat::PAIRED)
    thermostat.setTemperature(centi,sampledAt);
  portEXIT_CRITICAL(&thermostatMux);
}

void httpOtPairing(){
  if(!authorized())return;
  if(web.method()==HTTP_GET){otGateway.sendBindingStatus(web,radioReady);return;}
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="opentherm-pairing" ||
     web.arg("confirm")!="manual-1fc9-additional-controller"){
    web.send(403,"application/json",
      "{\"error\":\"explicit_pairing_confirmation_required\"}");return;
  }
  if(!configuration.current.webUser[0] ||
     strlen(configuration.current.webPassword)<12 ||
     strcmp(configuration.current.webPassword,
            "CHANGE_TO_A_STRONG_PRIVATE_PASSWORD")==0){
    web.send(403,"application/json","{\"error\":\"strong_web_auth_required\"}");
    return;
  }
  const String virtualId=web.arg("virtual_controller_id");
  if(virtualId.length()<1 || virtualId.length()>6){
    web.send(400,"application/json","{\"error\":\"invalid_virtual_controller_id\"}");
    return;
  }
  for(size_t i=0;i<virtualId.length();i++)
    if(virtualId[i]<'0'||virtualId[i]>'9'){
      web.send(400,"application/json","{\"error\":\"invalid_virtual_controller_id\"}");
      return;
    }
  const uint32_t id=static_cast<uint32_t>(virtualId.toInt());
  portENTER_CRITICAL(&thermostatMux);
  const bool thermostatBusy=thermostat.busy();
  portEXIT_CRITICAL(&thermostatMux);
  if(!radioReady || thermostatBusy || !otGateway.requestBinding(id)){
    web.send(409,"application/json",
      "{\"error\":\"pairing_not_ready_or_in_cooldown_or_address_conflict\"}");
    return;
  }
  recordLog(GatewayDebug::WARN,
    "Manual 1FC9 additional-controller binding queued: one offer, one confirmation if accepted, one read verification");
  web.send(202,"application/json",
    "{\"queued\":true,\"protocol\":\"1FC9\",\"mode\":\"experimental\"}");
}
void httpPairedBridgeRead() {
  if(web.method()==HTTP_GET) {
    if(!authorized())return;
    otGateway.sendPairedBridgeReadStatus(web,radioReady);
    return;
  }
  if(!strongThermostatAuth())return;
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="paired-bridge-read" ||
     web.arg("confirm")!="one-read-own-r8810a-as-observed-controller") {
    web.send(403,"application/json",
      "{\"error\":\"explicit_bridge_read_authorization_required\"}");
    return;
  }
  const String text=web.arg("data_id");
  if(text.isEmpty() || text.length()>3) {
    web.send(400,"application/json","{\"error\":\"invalid_opentherm_id\"}");
    return;
  }
  for(size_t i=0;i<text.length();i++)if(text[i]<'0'||text[i]>'9'){
    web.send(400,"application/json","{\"error\":\"invalid_opentherm_id\"}");
    return;
  }
  const uint32_t id=static_cast<uint32_t>(text.toInt());
  if(id>255 || !ramses_bridge_read::supportedId(static_cast<uint8_t>(id))) {
    web.send(400,"application/json","{\"error\":\"read_id_not_allowlisted\"}");
    return;
  }
  if(!radioReady || !otGateway.requestPairedBridgeRead(static_cast<uint8_t>(id))) {
    const char* reason=radioReady?otGateway.pairedBridgeReadBlockReason():"radio_not_ready";
    web.send(409,"application/json",
      String("{\"error\":\"")+(reason?reason:"bridge_read_busy")+"\"}");
    return;
  }
  char line[130];
  snprintf(line,sizeof(line),
    "Manual R8810A 3220 Read-Data ID%u queued as observed Evohome identity (no bind/write)",
    static_cast<unsigned>(id));
  recordLog(GatewayDebug::WARN,line);
  web.send(202,"application/json",
    "{\"queued\":true,\"request_count\":1,\"read_only\":true,\"identity\":\"observed_existing_01\",\"cooldown_seconds\":15}");
}
// Manual CH snapshot: exactly three sequential read-only OpenTherm requests.
// Session and results live on ESP32, not in the browser or persistent settings.
void httpCvReadBatch(){
  if(web.method()==HTTP_GET){
    if(!authorized())return;
    otGateway.sendCvBatchStatus(web,radioReady);
    return;
  }
  if(!strongThermostatAuth())return;
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="cv-read-batch" ||
     web.arg("confirm")!="sixteen-read-only-requests-existing-evohome-identity"){
    web.send(403,"application/json",
      "{\"error\":\"explicit_cv_read_authorization_required\"}");
    return;
  }
  if(!radioReady || !otGateway.requestCvBatch()){
    const char* reason=radioReady?otGateway.pairedBridgeReadBlockReason():"radio_not_ready";
    web.send(409,"application/json",
      String("{\"error\":\"")+(reason?reason:"cv_read_busy")+"\"}");
    return;
  }
  recordLog(GatewayDebug::WARN,
    "Manual R8810A diagnostic scan queued: 16 read-only IDs, >=15s spacing, observed Evohome identity");
  web.send(202,"application/json",
    "{\"queued\":true,\"read_only\":true,\"requests_max\":16,\"minimum_spacing_ms\":15000}");
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
  const String choice=web.arg("method");
  ot_tx::Method method=ot_tx::ASYNC_UART;
  if(choice=="fifo")method=ot_tx::REFERENCE_FIFO;
  else if(choice!="uart"){
    web.send(400,"application/json",
      "{\"error\":\"one_shot_method_must_be_uart_or_fifo\"}");
    return;
  }
  if(!radioReady || !otGateway.requestOneShot(method)){
    web.send(409,"application/json",
      "{\"error\":\"one_shot_not_ready_or_in_cooldown_check_status\"}");
    return;
  }
  recordLog(GatewayDebug::INFO,
     method==ot_tx::REFERENCE_FIFO?
       "OpenTherm one-shot queued: reference-primed 60-byte fixed FIFO TX":
       "OpenTherm one-shot queued: legacy asynchronous UART TX");
  web.send(202,"application/json",
    method==ot_tx::REFERENCE_FIFO?
    "{\"queued\":true,\"data_id\":25,\"attempt_limit\":1,\"method\":\"fifo_reference_primed_60\"}":
    "{\"queued\":true,\"data_id\":25,\"attempt_limit\":1,\"method\":\"async_uart\"}");
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
  if(configuration.unchanged(staged,GW_CC1101_MODULE_BAND)) {
    // Do not restart Wi-Fi/MQTT or rewrite flash for a no-op settings save.
    web.sendHeader("Cache-Control","no-store");
    web.send(200,"application/json","{\"saved\":true,\"unchanged\":true,\"restart_in_seconds\":0}");
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
           "\"cc1101_init_stage\":\"%s\",\"cc1101_boot_marcstate\":%u,\"cc1101_rx_at_boot\":%s,"
           "\"wifi_connected\":%s,\"mqtt_connected\":%s,\"time_synced\":%s,"
           "\"hostname\":\"%s\",\"mdns_active\":%s,\"cc1101_frequency_khz\":%lu,\"cc1101_module_band\":%u,\"ot_tx_enabled\":%s,"
           "\"valid_frames\":%lu,\"rejected_frames\":%lu,\"dropped_frames\":%lu,"
           "\"rf_uart_bytes\":%lu,\"rf_syncs\":%lu,\"radio_recalibrations\":%lu,"
           "\"radio_recalibration_failures\":%lu,\"radio_silence_warnings\":%lu,"
           "\"http_last_ms\":%lu,\"http_max_ms\":%lu,\"http_slow_calls\":%lu,\"http_serviced\":%lu,"
           "\"http_unserved_slow_calls\":%lu,\"http_unserved_max_ms\":%lu,"
           "\"mqtt_connect_max_ms\":%lu,\"mqtt_connect_slow_calls\":%lu,"
           "\"mqtt_loop_max_ms\":%lu,\"mqtt_loop_slow_calls\":%lu,"
           "\"web_asset_last_ms\":%lu,\"web_asset_max_ms\":%lu,"
           "\"last_frame_uptime_ms\":%lu,\"last_rssi\":%u,"
           "\"uptime_ms\":%lu,\"free_heap\":%lu,\"gateway_id\":\"%s\","
           "\"ip\":\"%s\",\"ap_ip\":\"%s\",\"ap_ssid\":\"%s\",\"reset_reason\":\"%s\"}",
           FW_VERSION,
           otGateway.transmitEnabled() ? "read_only_requests" : "receive_only",
           radioReady ? "true" : "false",
           validFrames ? "true" : "false", radio.part(), radio.version(),
           radio.initStage(),static_cast<unsigned>(radio.startupMarcState()),
           radio.startupRxConfirmed()?"true":"false",
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
           static_cast<unsigned long>(radioSilenceWarnings),
           static_cast<unsigned long>(httpLatency.last_ms),
           static_cast<unsigned long>(httpLatency.max_ms),
           static_cast<unsigned long>(httpLatency.slow_calls),
           static_cast<unsigned long>(httpServicedCount),
           static_cast<unsigned long>(httpUnservedSlowCalls),
           static_cast<unsigned long>(httpUnservedMaxMs),
           static_cast<unsigned long>(mqttConnectLatency.max_ms),
           static_cast<unsigned long>(mqttConnectLatency.slow_calls),
           static_cast<unsigned long>(mqttLoopLatency.max_ms),
           static_cast<unsigned long>(mqttLoopLatency.slow_calls),
           static_cast<unsigned long>(webAssetLastMs),
           static_cast<unsigned long>(webAssetMaxMs),
           static_cast<unsigned long>(lastFrameMs), lastRssi,
           static_cast<unsigned long>(millis()),
           static_cast<unsigned long>(ESP.getFreeHeap()), deviceId,
           ip.c_str(), apIp.c_str(), recoveryApReady ? recoveryApSsid : "",
           resetReason());
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", json);
}

// Serializes previously recorded data; never accesses SPI on the HTTP task.
void formatRxHealthSample(const ramses_rx_health::Sample& s,char* out,size_t cap) {
  snprintf(out,cap,
    "{\"uptime_ms\":%lu,\"uart_bytes\":%lu,\"marcstate\":%u,"
    "\"in_rx\":%s,\"iocfg2\":%u,\"pktctrl0\":%u,"
    "\"settings_match\":%s,\"gdo2_level\":%u}",
    static_cast<unsigned long>(s.uptime_ms),
    static_cast<unsigned long>(s.uart_bytes),
    static_cast<unsigned>(s.marcstate),s.inRx()?"true":"false",
    static_cast<unsigned>(s.iocfg2),static_cast<unsigned>(s.pktctrl0),
    s.settingsMatch()?"true":"false",static_cast<unsigned>(s.gdo2_level));
}
// Passive UART evidence: distinguishes an inactive GDO2/UART path from
// byte stream corruption without changing CC1101 registers or sending RF.
void httpUartProbe() {
  if(!authorized())return;
  ramses_uart_probe::Snapshot snapshot;
  portENTER_CRITICAL(&uartProbeMux);
  snapshot=uartProbe;
  portEXIT_CRITICAL(&uartProbeMux);
  char hex[2*ramses_uart_probe::CAPACITY+1]={};
  snapshot.toHex(hex,sizeof(hex));
  char response[275];
  const int n=snprintf(response,sizeof(response),
    "{\"uart_bytes\":%lu,\"zeros\":%lu,\"all_ones\":%lu,"
    "\"last_bytes_hex\":\"%s\",\"sample_capacity\":%u,"
    "\"note\":\"UART1 RX data only; does not prove RF reception\"}",
    static_cast<unsigned long>(snapshot.received),
    static_cast<unsigned long>(snapshot.zeros),
    static_cast<unsigned long>(snapshot.all_ones),hex,
    static_cast<unsigned>(ramses_uart_probe::CAPACITY));
  if(n<0 || static_cast<size_t>(n)>=sizeof(response)) {
    web.send(500,"application/json","{\"error\":\"uart_probe_overflow\"}");
    return;
  }
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json",response);
}
// HTTP only copies the audit; CC1101 SPI remains exclusive to radioTask.
void httpSpiAudit() {
  if(!authorized())return;
  ramses_spi_audit::Snapshot s;
  uint32_t count=0;
  portENTER_CRITICAL(&spiAuditMux);
  s=spiAudit;
  count=spiAuditSamples;
  portEXIT_CRITICAL(&spiAuditMux);
  char output[768];
  if(!ramses_spi_audit::formatAuditJson(s,count,output,sizeof(output))) {
    web.send(500,"application/json","{\"error\":\"spi_audit_overflow\"}");
    return;
  }
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json",output);
}
void httpRxHealth() {
  if(!authorized())return;
  ramses_rx_health::Report d;
  portENTER_CRITICAL(&rxHealthMux);
  d=rxHealth;
  portEXIT_CRITICAL(&rxHealthMux);
  char latest[220],before[220],after[220],body[1150];
  formatRxHealthSample(d.latest,latest,sizeof(latest));
  formatRxHealthSample(d.before_recalibration,before,sizeof(before));
  formatRxHealthSample(d.after_recalibration,after,sizeof(after));
  const int n=snprintf(body,sizeof(body),
    "{\"ready\":%s,\"read_only\":true,"
    "\"note\":\"GPIO27 level is instantaneous, not a waveform or RF packet\","
    "\"expected\":{\"marcstate_rx\":13,\"iocfg2\":13,\"pktctrl0\":50},"
    "\"sample_count\":%lu,\"samples_not_in_rx\":%lu,"
    "\"samples_config_mismatch\":%lu,\"gpio_high_samples\":%lu,"
    "\"gpio_low_samples\":%lu,\"recalibrations_sampled\":%lu,"
    "\"recalibrations_not_in_rx\":%lu,\"has_recalibration\":%s,"
    "\"last_recalibration_spi_ok\":%s,"
    "\"latest\":%s,\"before_recalibration\":%s,\"after_recalibration\":%s}",
    radioReady && d.has_latest?"true":"false",
    static_cast<unsigned long>(d.samples),
    static_cast<unsigned long>(d.samples_not_in_rx),
    static_cast<unsigned long>(d.samples_config_mismatch),
    static_cast<unsigned long>(d.gpio_high_samples),
    static_cast<unsigned long>(d.gpio_low_samples),
    static_cast<unsigned long>(d.recalibrations_sampled),
    static_cast<unsigned long>(d.recalibrations_not_in_rx),
    d.has_recalibration?"true":"false",
    d.last_recalibration_spi_ok?"true":"false",
    latest,d.has_recalibration?before:"null",
    d.has_recalibration?after:"null");
  if(n<0 || static_cast<size_t>(n)>=sizeof(body)) {
    web.send(500,"application/json","{\"error\":\"rx_health_json_overflow\"}");
    return;
  }
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json",body);
}

// Authenticated passive RX diagnostics. Debug toggle changes only log output;
// safety-critical RF receive/TX behavior and configuration remain untouched.
void httpRadioDiagnostics() {
  if(!authorized())return;
  if(web.method()==HTTP_POST) {
    if(!web.hasHeader("X-Gateway-Config") ||
       web.header("X-Gateway-Config")!="radio-debug" ||
       (web.arg("enabled")!="1" && web.arg("enabled")!="0")) {
      web.send(403,"application/json",
        "{\"error\":\"explicit_radio_debug_confirmation_required\"}");
      return;
    }
    const bool enable=web.arg("enabled")=="1";
    rxDebugEnabled=enable;
    rxDebugEnabledAtMs=enable?millis():0;
    recordLog(GatewayDebug::INFO,enable?
      "RAMSES RX debug enabled (summary every 15s; no restart or RF TX)":
      "RAMSES RX debug disabled (no restart or RF TX)");
  }
  ramses_rx::Counters d;
  portENTER_CRITICAL(&rxDiagnosticsMux);
  d=rxDiagnostics;
  portEXIT_CRITICAL(&rxDiagnosticsMux);
  char body[1250];
  const uint32_t now=millis();
  const int written=snprintf(body,sizeof(body),
    "{\"debug_enabled\":%s,\"debug_since_uptime_ms\":%lu,"
    "\"note\":\"Passive diagnostics; invalid frames cannot be assigned to an RF sender\","
    "\"bridge_id\":%lu,\"requests_to_bridge\":%lu,"
    "\"messages_from_bridge\":%lu,\"bridge_3ef0\":%lu,"
    "\"bridge_3220\":%lu,\"last_bridge_uptime_ms\":%lu,"
    "\"last_bridge_age_ms\":%lu,\"last_request_uptime_ms\":%lu,"
    "\"last_request_age_ms\":%lu,"
    "\"rejected\":{\"checksum\":%lu,\"invalid_manchester\":%lu,"
    "\"malformed_structure\":%lu,\"oversized\":%lu,"
    "\"incomplete\":%lu,\"idle_timeout\":%lu,"
    "\"format_failure\":%lu},"
    "\"last_rejection\":\"%s\",\"last_rejection_uptime_ms\":%lu,"
    "\"last_rejection_age_ms\":%lu,"
    "\"rssi_note\":\"CC1101 RSSI is read after frame, indicative not calibrated\","
    "\"last_rssi_magnitude\":%u,\"valid_frames\":%lu,\"rejected_frames\":%lu}",
    rxDebugEnabled?"true":"false",
    static_cast<unsigned long>(rxDebugEnabledAtMs),
    static_cast<unsigned long>(configuration.current.otBridge),
    static_cast<unsigned long>(d.requestsToBridge),
    static_cast<unsigned long>(d.messagesFromBridge),
    static_cast<unsigned long>(d.bridge3ef0),
    static_cast<unsigned long>(d.bridge3220),
    static_cast<unsigned long>(d.lastBridgeMs),
    static_cast<unsigned long>(d.lastBridgeMs?now-d.lastBridgeMs:0),
    static_cast<unsigned long>(d.lastRequestMs),
    static_cast<unsigned long>(d.lastRequestMs?now-d.lastRequestMs:0),
    static_cast<unsigned long>(d.checksum),
    static_cast<unsigned long>(d.invalidSymbol),
    static_cast<unsigned long>(d.malformed),
    static_cast<unsigned long>(d.oversized),
    static_cast<unsigned long>(d.incomplete),
    static_cast<unsigned long>(d.idleTimeout),
    static_cast<unsigned long>(d.formatFailure),
    ramses::rejectName(d.lastReject),
    static_cast<unsigned long>(d.lastRejectMs),
    static_cast<unsigned long>(d.lastRejectMs?now-d.lastRejectMs:0),
    static_cast<unsigned>(lastRssi),
    static_cast<unsigned long>(validFrames),
    static_cast<unsigned long>(rejectedFrames));
  if(written<0 || static_cast<size_t>(written)>=sizeof(body)){
    web.send(500,"application/json","{\"error\":\"rx_diagnostics_overflow\"}");
    return;
  }
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json",body);
}

// These endpoints never alter the radio or transmit. The capture is RAM-only
// (not raw RF bits) and automatically freezes at 192 events. Export before
// starting a new capture, clearing, or restarting.
void httpCaptureStatus(){
  if(!authorized())return;
  bool active,full;
  size_t count;
  uint32_t started,stopped,received,rejected,uartStart,syncStart,droppedStart;
  portENTER_CRITICAL(&packetCaptureMux);
  active=packetCapture.active;full=packetCapture.full;
  count=packetCapture.count;
  started=packetCapture.started_ms;stopped=packetCapture.stopped_ms;
  received=packetCapture.received;rejected=packetCapture.rejected;
  uartStart=packetCapture.start_uart_bytes;
  syncStart=packetCapture.start_syncs;
  droppedStart=packetCapture.start_dropped;
  portEXIT_CRITICAL(&packetCaptureMux);
  char out[810];
  const int len=snprintf(out,sizeof(out),
    "{\"active\":%s,\"full\":%s,\"storage\":\"RAM_only\","
    "\"limit\":%u,\"count\":%u,\"decoded\":%lu,\"rejected\":%lu,"
    "\"started_uptime_ms\":%lu,\"stopped_uptime_ms\":%lu,"
    "\"radio_uart_bytes_since_start\":%lu,\"syncs_since_start\":%lu,"
    "\"main_queue_dropped_since_start\":%lu,"
    "\"notes\":\"Only checksum-valid packets and decoder-rejected frame candidates; not raw RF bits. Capture stops when full. Export before restart or clear.\"}",
    active?"true":"false",full?"true":"false",
    static_cast<unsigned>(ramses_capture::MAX_ITEMS),
    static_cast<unsigned>(count),
    static_cast<unsigned long>(received),
    static_cast<unsigned long>(rejected),
    static_cast<unsigned long>(started),
    static_cast<unsigned long>(stopped),
    static_cast<unsigned long>(started?rfUartBytes-uartStart:0),
    static_cast<unsigned long>(started?rfSyncs-syncStart:0),
    static_cast<unsigned long>(started?droppedFrames-droppedStart:0));
  if(len<=0 || static_cast<size_t>(len)>=sizeof(out)){
    web.send(500,"application/json","{\"error\":\"status_overflow\"}");return;
  }
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json",out);
}

void httpCaptureAction(){
  if(!authorized())return;
  if(!web.hasHeader("X-Gateway-Config") ||
     web.header("X-Gateway-Config")!="packet-capture" ||
     !web.hasArg("action")) {
    web.send(403,"application/json","{\"error\":\"capture_confirmation_required\"}");
    return;
  }
  const String action=web.arg("action");
  if(action!="start" && action!="stop" && action!="clear"){
    web.send(400,"application/json","{\"error\":\"unknown_capture_action\"}");
    return;
  }
  const uint32_t now=millis();
  portENTER_CRITICAL(&packetCaptureMux);
  if(action=="start")packetCapture.start(now,rfUartBytes,rfSyncs,droppedFrames);
  if(action=="stop")packetCapture.stop(now);
  if(action=="clear")packetCapture.clear();
  portEXIT_CRITICAL(&packetCaptureMux);
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json","{\"ok\":true}");
}

// Read each entry under a short lock, send outside lock to avoid blocking RX.
// On-core HTTP is serialized; start/clear cannot interfere mid-download.
void httpCaptureDownload(){
  if(!authorized())return;
  size_t count;
  uint32_t started,stopped;
  bool active,full;
  portENTER_CRITICAL(&packetCaptureMux);
  count=packetCapture.count;
  started=packetCapture.started_ms;
  stopped=packetCapture.stopped_ms;
  active=packetCapture.active;
  full=packetCapture.full;
  portEXIT_CRITICAL(&packetCaptureMux);
  char filename[72];
  snprintf(filename,sizeof(filename),"attachment; filename=RAMSES-RX-%lu.tsv",
           static_cast<unsigned long>(started));
  web.sendHeader("Content-Disposition",filename);
  web.sendHeader("Cache-Control","no-store");
  web.setContentLength(CONTENT_LENGTH_UNKNOWN);
  web.send(200,"text/tab-separated-values; charset=utf-8","");
  char line[380];
  snprintf(line,sizeof(line),
    "# RAMSES RX capture: start_uptime_ms=%lu stop_uptime_ms=%lu active_at_export=%u full=%u entries=%u\n",
    static_cast<unsigned long>(started),static_cast<unsigned long>(stopped),
    static_cast<unsigned>(active),static_cast<unsigned>(full),
    static_cast<unsigned>(count));
  web.sendContent(line);
  web.sendContent("# Times are device uptime in ms; date anchor at export follows. No raw RF bits; receiver may miss frames.\n");
  char anchor[40];
  makeTimestamp(anchor,sizeof(anchor));
  snprintf(line,sizeof(line),"# export_utc=%s export_uptime_ms=%lu\n",
           anchor,static_cast<unsigned long>(millis()));
  web.sendContent(line);
  web.sendContent("sequence\tlocal_time\tutc_epoch_ms\tuptime_ms\ttype\trssi_magnitude\tmessage\n");
  for(size_t i=0;i<count;i++){
    ramses_capture::Entry row;
    portENTER_CRITICAL(&packetCaptureMux);
    const bool present=packetCapture.item(i,row);
    portEXIT_CRITICAL(&packetCaptureMux);
    if(!present)break;
    // Input strings are generated internally (no tab/CR/LF).
    char localStamp[48]="unsynced";
    if(row.utc_ms>0) {
      const time_t seconds=static_cast<time_t>(row.utc_ms/1000u);
      tm local={};
      localtime_r(&seconds,&local);
      if(!strftime(localStamp,sizeof(localStamp),
                   "%Y-%m-%dT%H:%M:%S%z",&local))
        snprintf(localStamp,sizeof(localStamp),"time_format_error");
    }
    const int n=snprintf(line,sizeof(line),"%lu\t%s\t%llu\t%lu\t%s\t%u\t%s\n",
      static_cast<unsigned long>(row.sequence),localStamp,
      static_cast<unsigned long long>(row.utc_ms),
      static_cast<unsigned long>(row.uptime_ms),
      ramses_capture::kindName(row.kind),
      static_cast<unsigned>(row.rssi),row.message);
    if(n>0 && static_cast<size_t>(n)<sizeof(line))web.sendContent(line);
    if((i&15u)==0)delay(0); // yield while serving a full capture
  }
  web.sendContent("");
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
  if(!authorized())return;
  const uint32_t cursor=static_cast<uint32_t>(strtoul(web.arg("since").c_str(),nullptr,10));
  const int level=constrain(web.arg("level").toInt(),0,3);
  const auto maxLevel=static_cast<GatewayDebug::Level>(web.hasArg("level")?level:2);
  // Limit response to 20 entries (<~5KB) and one TCP write. The former
  // handler issued one synchronous sendContent per entry, starving the UI.
  constexpr uint8_t MAX_SCAN=20;
  String json;
  if(!json.reserve(5800)){
    web.send(503,"application/json","{\"error\":\"low_memory\"}");
    return;
  }
  json="{\"next\":";
  String entries;
  if(!entries.reserve(5600)){
    web.send(503,"application/json","{\"error\":\"low_memory\"}");
    return;
  }
  uint32_t next=cursor;
  uint8_t inspected=0;
  diagnosticLog.eachAfter(cursor,GatewayDebug::TRACE,[&](const GatewayDebug::Entry& entry) {
    if(inspected>=MAX_SCAN)return;
    ++inspected;
    next=entry.sequence;
    if(entry.level>maxLevel)return;
    if(entries.length())entries+=',';
    entries+="{\"seq\":";
    entries+=String(entry.sequence);
    entries+=",\"uptime_ms\":";
    entries+=String(entry.uptimeMs);
    entries+=",\"timestamp\":";
    char stamp[52]={};
    if(eventLocalTime(entry.uptimeMs,stamp,sizeof(stamp)))
      entries+="\""+escapeJSON(stamp)+"\"";
    else entries+="null";
    entries+=",\"level\":\"";
    entries+=GatewayDebug::levelName(entry.level);
    entries+="\",\"message\":\"";
    entries+=escapeJSON(entry.message);
    entries+="\"}";
  });
  json+=String(next);
  json+=",\"entries\":[";
  json+=entries;
  json+="]}";
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json",json);
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

// Bound individual Wi-Fi writes: the previous send_P() attempted a 41KB
// page in one response, and the browser could remain stuck before <script>.
// Separate HTML/JS assets and send small flash-backed blocks with yields.
// Content-Length is exact, so clients never wait for an unfinished chunk.
void sendFlashAsset(const char* data,size_t length,const char* mime){
  const uint32_t transferStart=millis();
  web.client().setNoDelay(true);
  web.sendHeader("Cache-Control","no-store");
  web.sendHeader("X-Content-Type-Options","nosniff");
  web.setContentLength(length);
  web.send(200,mime,"");
  constexpr size_t BLOCK=1024;
  for(size_t pos=0;pos<length;pos+=BLOCK){
    const size_t n=(length-pos)<BLOCK?(length-pos):BLOCK;
    web.sendContent_P(data+pos,n);
    delay(0); // allow TCP/IP task and Wi-Fi to drain between blocks
  }
  webAssetLastMs=millis()-transferStart;
  if(webAssetLastMs>webAssetMaxMs)webAssetMaxMs=webAssetLastMs;
}
void httpIndex() {
  if(!authorized())return;
  sendFlashAsset(GW_DEBUG_PAGE,sizeof(GW_DEBUG_PAGE)-1u,"text/html; charset=utf-8");
}
void httpGatewayJs(){
  if(!authorized())return;
  sendFlashAsset(GW_WEB_JS,sizeof(GW_WEB_JS)-1u,"application/javascript; charset=utf-8");
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
    if(!mqtt.subscribe(thermostatTopic))recordLog(GatewayDebug::WARN,"MQTT thermostat temperature subscription failed");
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
  Serial.println("\n# RAMSES II Arduino v0.4.0-beta.37 (read-only OpenTherm discovery)");
  recordLog(GatewayDebug::INFO, "Boot: RAMSES gateway v0.4.0-beta.37");
  char bootInfo[96];
  snprintf(bootInfo, sizeof(bootInfo), "Reset reason: %s", resetReason());
  recordLog(GatewayDebug::INFO, bootInfo);
  Serial.printf("# Core=%lu MHz, flash=%lu bytes\n",
                static_cast<unsigned long>(getCpuFrequencyMhz()), static_cast<unsigned long>(ESP.getFlashChipSize()));
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
  snprintf(thermostatTopic, sizeof(thermostatTopic), "%s/thermostat/temperature/set", baseTopic);

  if (strlen(configuration.current.recoveryPassword) >= 8) {
    WiFi.mode(WIFI_AP_STA);
    snprintf(recoveryApSsid, sizeof(recoveryApSsid), "RAMSES-Debug-%06lX", static_cast<unsigned long>(serial));
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
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(1536); // Home Assistant Discovery configurations are larger than 512 bytes
  mqtt.setSocketTimeout(1); // bound socket read stalls; HTTP runs on same loop()
  web.on("/", HTTP_GET, httpIndex);
  web.on("/gateway.js", HTTP_GET, httpGatewayJs);
  web.on("/api/v1/status", HTTP_GET, httpStatus);
  web.on("/api/v1/config", HTTP_GET, httpConfigGet);
  web.on("/api/v1/config", HTTP_POST, httpConfigSave);
  web.on("/api/v1/packets", HTTP_GET, httpPackets);
  web.on("/api/v1/capture/status", HTTP_GET, httpCaptureStatus);
  web.on("/api/v1/capture/action", HTTP_POST, httpCaptureAction);
  web.on("/api/v1/capture/download", HTTP_GET, httpCaptureDownload);
  web.on("/api/v1/radio/rx-health", HTTP_GET, httpRxHealth);
  web.on("/api/v1/radio/spi-audit", HTTP_GET, httpSpiAudit);
  web.on("/api/v1/radio/uart-probe", HTTP_GET, httpUartProbe);
  web.on("/api/v1/radio/diagnostics", HTTP_GET, httpRadioDiagnostics);
  web.on("/api/v1/radio/diagnostics", HTTP_POST, httpRadioDiagnostics);
  web.on("/api/v1/opentherm/profile", HTTP_GET, httpOtProfile);
  web.on("/api/v1/thermostat", HTTP_GET, httpThermostatStatus);
  web.on("/api/v1/thermostat/pairing", HTTP_POST, httpThermostatPairing);
  web.on("/api/v1/thermostat/forget", HTTP_POST, httpThermostatForget);
  web.on("/api/v1/opentherm/pairing/forget", HTTP_POST, httpVirtualControllerForget);
  web.on("/api/v1/thermostat/temperature", HTTP_POST, httpThermostatTemperature);
  web.on("/api/v1/opentherm/pairing", HTTP_GET, httpOtPairing);
  web.on("/api/v1/opentherm/pairing", HTTP_POST, httpOtPairing);
  web.on("/api/v1/opentherm/paired-read", HTTP_GET, httpPairedBridgeRead);
  web.on("/api/v1/opentherm/paired-read", HTTP_POST, httpPairedBridgeRead);
  web.on("/api/v1/opentherm/cv-batch", HTTP_GET, httpCvReadBatch);
  web.on("/api/v1/opentherm/cv-batch", HTTP_POST, httpCvReadBatch);
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
  // HTTP first: MQTT reconnect and network socket timeouts must not
  // delay an already queued browser request in the same loop pass.
  uint32_t started=millis();
  const uint32_t handledBefore=httpServicedCount;
  web.handleClient();
  const uint32_t httpElapsed=static_cast<uint32_t>(millis()-started);
  if(httpServicedCount!=handledBefore) {
    httpLatency.observe(httpElapsed);
  } else if(httpElapsed>=500u) {
    // WebServer can block while parsing an incomplete client request
    // without dispatching an endpoint; keep it separate from handler latency.
    ++httpUnservedSlowCalls;
    if(httpElapsed>httpUnservedMaxMs)httpUnservedMaxMs=httpElapsed;
  }
  started=millis();
  mqttConnect();
  mqttConnectLatency.observe(static_cast<uint32_t>(millis()-started));
  if (mqtt.connected()) {
    static uint32_t lastMqttLoopMs=0;
    if(millis()-lastMqttLoopMs>=100u){
      lastMqttLoopMs=millis();
      started=millis();
      mqtt.loop();
      mqttLoopLatency.observe(static_cast<uint32_t>(millis()-started));
    }
    if (millis() - lastMqttStateMs >= MQTT_STATE_INTERVAL_MS) {
      lastMqttStateMs = millis();
      if (!publishGatewayState())
        recordLog(GatewayDebug::WARN, "MQTT state publish failed");
    }
  }
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
    ramses_rx_health::Sample lastRxSample;
    bool rxSampleValid=false;
    portENTER_CRITICAL(&rxHealthMux);
    lastRxSample=rxHealth.latest;
    rxSampleValid=rxHealth.has_latest;
    portEXIT_CRITICAL(&rxHealthMux);
    char heartbeat[270];
    snprintf(heartbeat,sizeof(heartbeat),
             "HEARTBEAT CC1101=%s RX=%s MARC=%u UART=%lu sync=%lu valid=%lu rejected=%lu heap=%lu HTTPmax=%lums MQTTconnectMax=%lums MQTTloopMax=%lums",
             radioReady?"ok":"missing",
             !rxSampleValid?"unknown":lastRxSample.marcstate==0xFFu?"spi_unreadable":lastRxSample.inRx()?"active":"not_in_rx",
             static_cast<unsigned>(lastRxSample.marcstate),
             static_cast<unsigned long>(rfUartBytes),
             static_cast<unsigned long>(rfSyncs),
             static_cast<unsigned long>(validFrames),
             static_cast<unsigned long>(rejectedFrames),
             static_cast<unsigned long>(ESP.getFreeHeap()),
             static_cast<unsigned long>(httpLatency.max_ms),
             static_cast<unsigned long>(mqttConnectLatency.max_ms),
             static_cast<unsigned long>(mqttLoopLatency.max_ms));
    recordLog(GatewayDebug::INFO, heartbeat);
  }
  delay(2);
}
