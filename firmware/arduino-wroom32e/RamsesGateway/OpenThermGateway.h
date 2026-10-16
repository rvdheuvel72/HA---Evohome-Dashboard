// Integration of read-only RAMSES-II OpenTherm discovery, NVS profile and
// conservative RF request scheduling for ESP32 + CC1101. All SPI/UART radio
// operations must execute on the existing dedicated radioTask.
// This is an experimental implementation; RF TX needs bench verification.
#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <WebServer.h>
#include "OpenThermScan.h"
#include "OpenThermOneShot.h"
#include "RamsesReadRequest.h"
#include "Cc1101Radio.h"
#include "GatewayRuntimeConfig.h"
#include "RamsesCodec.h"

class OpenThermGateway {
 public:
  void configure(const gw_config::Settings* settings,uint16_t band){config_=settings;moduleBand_=band;}
  bool transmitConfigured() const {
    return config_ && config_->otTxEnabled == 1 &&
        config_->otBridge > 0 && config_->otBridge <= 262143 &&
        config_->otSource > 0 && config_->otSource < 262143 &&
        config_->otSource != 730 &&
        config_->rfExperimentalRx == 0 &&
        radio_frequency::evohomeTxFrequency(config_->rfFrequencyKhz,
                                              moduleBand_);
  }
  bool transmitEnabled() const { return transmitConfigured() && bridgeSeen_; }
  // One-shot exception NEVER arms the existing scan/poll TX path.
  bool experimentalOneShotReady() const {
    return config_ && config_->otTxEnabled==0 && config_->otAutoPoll==0 &&
      config_->otScanAll==0 &&
      config_->otBridge>0 && config_->otBridge<=262143 &&
      config_->otSource>0 && config_->otSource<262143 &&
      config_->otSource!=730 && bridgeSeen_ && !sourceConflict_ &&
      radio_frequency::experimentalOneShotFrequency(
        config_->rfFrequencyKhz,config_->rfModuleBand,
        config_->rfExperimentalRx==1);
  }
  bool requestOneShot() {
    if(!experimentalOneShotReady() || work_.active || requested_)return false;
    portENTER_CRITICAL(&mutex_);
    const bool ok=singleProbe_.request(millis());
    portEXIT_CRITICAL(&mutex_);
    return ok;
  }
  void sendOneShotStatus(WebServer& server) {
    ot_single::Probe snapshot;
    portENTER_CRITICAL(&mutex_);
    snapshot=singleProbe_;
    portEXIT_CRITICAL(&mutex_);
    float degrees=0;
    const bool hasValue=snapshot.state==ot_single::READ_ACK &&
      ot_readonly::fixedPoint(ot_single::DATA_ID,snapshot.raw,degrees);
    char response[550];
    snprintf(response,sizeof(response),
      "{\"data_id\":%u,\"name\":\"boiler_flow_c\",\"status\":\"%s\","
      "\"ready\":%s,\"bridge_seen\":%s,\"source_conflict\":%s,"
      "\"bridge_id\":%lu,\"source_id\":%lu,\"cooldown_ms\":%lu,"
      "\"attempted\":%s,\"raw\":%u,\"value_c\":%.3f,\"has_value\":%s}",
      static_cast<unsigned>(ot_single::DATA_ID),snapshot.label(),
      experimentalOneShotReady()?"true":"false",
      bridgeSeen_?"true":"false",sourceConflict_?"true":"false",
      static_cast<unsigned long>(config_->otBridge),
      static_cast<unsigned long>(config_->otSource),
      static_cast<unsigned long>(snapshot.cooldown(millis())),
      snapshot.attempted?"true":"false",
      static_cast<unsigned>(snapshot.raw),
      static_cast<double>(hasValue?degrees:0),hasValue?"true":"false");
    server.sendHeader("Cache-Control","no-store");
    server.send(200,"application/json",response);
  }
  // Only RX-verified packets can establish the bridge identity. A configured
  // RF address alone is not permission to transmit into a heating system.
  void noteDevice(const ramses::Message& msg) {
    if(config_->otBridge && (msg.addrMask&1) &&
       rfType(msg.addresses[0])==10 &&
       rfId(msg.addresses[0])==config_->otBridge)
      bridgeSeen_=true;
    // Check RF *sender* only. Replies addressed to our virtual ID are
    // expected and must never be misclassified as an address collision.
    if((msg.addrMask&1u) && config_->otSource &&
       rfType(msg.addresses[0])==18 &&
       rfId(msg.addresses[0])==config_->otSource)
      sourceConflict_=true;
  }
  void begin() {
    Preferences p;
    if (p.begin("ot-probe",true)) {
      Persist stored = {};
      if (p.getBytesLength("profile")==sizeof(stored) &&
          p.getBytes("profile",&stored,sizeof(stored))==sizeof(stored) &&
          stored.magic==PROFILE_MAGIC &&
          stored.bridgeId==config_->otBridge &&
          stored.frequency==config_->rfFrequencyKhz) {
        work_.bridgeId=stored.bridgeId;
        work_.completed=true;
        for (unsigned i=0;i<256;i++) {
          const uint8_t state=stored.states[i];
          if(state>=ot_readonly::READ_ACK && state<=ot_readonly::TIMEOUT) {
            work_.entries[i].result=static_cast<ot_readonly::Result>(state);
            work_.entries[i].raw=stored.raw[i];
            work_.attempted++;
            if(state==ot_readonly::READ_ACK)work_.acknowledged++;
          }
        }
      }
      p.end();
    }
    publishSnapshot();
  }
  bool requestScan() {
    if(!transmitEnabled())return false;
    portENTER_CRITICAL(&mutex_);
    const bool ok=!published_.active && !requested_;
    if(ok)requested_=true;
    portEXIT_CRITICAL(&mutex_);
    return ok;
  }
  // Must be called from the radioTask after a checksum-validated frame.
  bool observe(const ramses::Message& msg,uint8_t& id,
               ot_readonly::Result& r,uint16_t& raw) {
    if(msg.opcode!=0x3220 || msg.messageType!=3 ||
       !(msg.addrMask&1) || rfType(msg.addresses[0])!=10 ||
       (config_->otBridge!=0 && rfId(msg.addresses[0])!=config_->otBridge) ||
       !ot_readonly::response(msg.payload,msg.length,id,r,raw))return false;
    // Only attribute scan results to our transaction when the reply
    // explicitly addresses the configured gateway RF identity.
    if((msg.addrMask&2) && rfType(msg.addresses[1])==18 &&
       rfId(msg.addresses[1])==config_->otSource) {
      portENTER_CRITICAL(&mutex_);
      singleProbe_.accept(id,r,raw,millis());
      portEXIT_CRITICAL(&mutex_);
      if(work_.accept(id,r,raw,millis()))publishSnapshot();
    }
    return true; // Still valid passive RF telemetry from configured bridge.
  }
  // Called from radioTask, never from HTTP/MQTT task.
  void tick(Cc1101Radio& radio,HardwareSerial& port,ramses::FrameDecoder& decoder) {
    bool start=false;
    portENTER_CRITICAL(&mutex_);
    if(requested_){start=true;requested_=false;}
    portEXIT_CRITICAL(&mutex_);
    const uint32_t now=millis();
    bool oneShot=false;
    portENTER_CRITICAL(&mutex_);
    oneShot=singleProbe_.begin(now);
    singleProbe_.tick(now);
    portEXIT_CRITICAL(&mutex_);
    if(oneShot) {
      const bool ok=experimentalOneShotReady() && !work_.active && !requested_ &&
        sendReadRequest(radio,port,decoder,ot_single::DATA_ID,true);
      portENTER_CRITICAL(&mutex_);
      singleProbe_.sent(ok);
      portEXIT_CRITICAL(&mutex_);
    }
    if(start) {
      work_.reset(config_->otBridge,config_ && config_->otScanAll==1,now);
      wasCompleted_=false;
      publishSnapshot();
    }
    if(work_.active) {
      const int id=work_.nextRequest(now);
      if(id>=0){
        if(!transmit(radio,port,decoder,static_cast<uint8_t>(id)))
          txFailures_++; // on failure, ID eventually becomes timeout, never "unsupported".
        publishSnapshot();
      }
    } else if(work_.completed && !wasCompleted_) {
      wasCompleted_=true;
      save();
      publishSnapshot();
    }
    // Normal polling is opt-in, derived ONLY from observed READ_ACK IDs.
    if(config_ && config_->otAutoPoll==1 && !work_.active && work_.completed &&
       static_cast<int32_t>(now-nextPolling_)>=0 && transmitEnabled()) {
      nextPolling_=now+60000; // at most one request per minute
      for(unsigned attempts=0;attempts<256;attempts++){
        const uint8_t id=pollCursor_++;
        if(work_.autoPoll(id)){
          if(!transmit(radio,port,decoder,id))txFailures_++;
          break;
        }
      }
    }
  }
  bool copyProfile(ot_scan::Scanner& copy) {
    portENTER_CRITICAL(&mutex_);
    copy=published_;
    portEXIT_CRITICAL(&mutex_);
    return copy.completed;
  }
  // Stream JSON instead of buffering all tested IDs in ESP32 heap.
  void sendProfile(WebServer& server) {
    ot_scan::Scanner copy;
    portENTER_CRITICAL(&mutex_);
    copy=published_;
    const bool requestPending=requested_;
    portEXIT_CRITICAL(&mutex_);
    server.sendHeader("Cache-Control","no-store");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200,"application/json","");
    char header[240];
    snprintf(header,sizeof(header),
      "{\"device\":\"R8810A\",\"bridge_id\":%lu,\"frequency_khz\":%lu,"
      "\"module_band\":%u,\"rf_tx_enabled\":%s,\"scan_active\":%s,"
      "\"scan_pending\":%s,\"scan_completed\":%s,\"full_scan\":%s,"
      "\"attempted\":%u,\"read_ack\":%u,\"tx_failures\":%lu,\"results\":[",
      static_cast<unsigned long>(config_->otBridge),
      static_cast<unsigned long>(config_->rfFrequencyKhz),
      static_cast<unsigned>(moduleBand_),
      transmitEnabled()?"true":"false",copy.active?"true":"false",
      requestPending?"true":"false",copy.completed?"true":"false",
      copy.full?"true":"false",copy.attempted,copy.acknowledged,
      static_cast<unsigned long>(txFailures_));
    server.sendContent(header);
    bool first=true;
    for (unsigned i=0;i<256;i++) {
      if(copy.entries[i].result==ot_readonly::NOT_TESTED)continue;
      if(!first)server.sendContent(",");
      first=false;
      const ot_readonly::Entry& e=copy.entries[i];
      char line[224];
      const bool enabled=copy.autoPoll(static_cast<uint8_t>(i));
      float value=0;
      const bool hasValue=e.result==ot_readonly::READ_ACK &&
           ot_readonly::fixedPoint(static_cast<uint8_t>(i),e.raw,value);
      if(hasValue)
        snprintf(line,sizeof(line),
          "{\"id\":%u,\"name\":\"%s\",\"result\":\"%s\","
          "\"raw\":%u,\"value\":%.3f,\"auto_poll\":%s}",
          i,ot_readonly::label(static_cast<uint8_t>(i)),
          ot_readonly::state(e.result),e.raw,
          static_cast<double>(value),enabled?"true":"false");
      else
        snprintf(line,sizeof(line),
          "{\"id\":%u,\"name\":\"%s\",\"result\":\"%s\","
          "\"raw\":%u,\"value\":null,\"auto_poll\":%s}",
          i,ot_readonly::label(static_cast<uint8_t>(i)),
          ot_readonly::state(e.result),e.raw,enabled?"true":"false");
      server.sendContent(line);
    }
    // Machine-readable configuration derived exclusively from observed
    // Read-Ack replies. This is a recommendation, not blanket authorization
    // to transmit: periodic polling is separately opt-in.
    server.sendContent("],\"recommended_poll_ids\":[");
    bool firstEligible=true;
    for(unsigned i=0;i<256;i++) {
      if(!copy.autoPoll(static_cast<uint8_t>(i)))continue;
      if(!firstEligible)server.sendContent(",");
      firstEligible=false;
      server.sendContent(String(i));
    }
    server.sendContent("],\"auto_poll_enabled\":");
    server.sendContent(config_ && config_->otAutoPoll==1?"true}":"false}");
    server.sendContent("");
  }
 private:
  struct Persist {
    uint32_t magic,bridgeId,frequency;
    uint8_t states[256];
    uint16_t raw[256];
  };
  static constexpr uint32_t PROFILE_MAGIC=0x4f543034;
  const gw_config::Settings* config_=nullptr;
  uint16_t moduleBand_=868;
  portMUX_TYPE mutex_=portMUX_INITIALIZER_UNLOCKED;
  ot_scan::Scanner work_,published_;
  bool requested_=false;
  volatile bool bridgeSeen_=false;
  volatile bool sourceConflict_=false;
  ot_single::Probe singleProbe_;
  bool wasCompleted_=false;
  uint32_t nextPolling_=0;
  uint32_t txFailures_=0;
  uint8_t pollCursor_=0;
  static uint8_t rfType(const uint8_t* a){return a[0]>>2;}
  static uint32_t rfId(const uint8_t* a){
    return (static_cast<uint32_t>(a[0]&3u)<<16)|
           (static_cast<uint32_t>(a[1])<<8)|a[2];
  }
  void publishSnapshot() {
    portENTER_CRITICAL(&mutex_);
    published_=work_;
    portEXIT_CRITICAL(&mutex_);
  }
  void save() {
    Persist data={};
    data.magic=PROFILE_MAGIC;
    data.bridgeId=config_->otBridge;
    data.frequency=config_->rfFrequencyKhz;
    for(unsigned i=0;i<256;i++){
      data.states[i]=work_.entries[i].result;
      data.raw[i]=work_.entries[i].raw;
    }
    Preferences p;
    if(p.begin("ot-probe",false)){
      p.putBytes("profile",&data,sizeof(data));
      p.end();
    }
  }
  bool transmit(Cc1101Radio& radio,HardwareSerial& port,
                ramses::FrameDecoder& decoder,uint8_t id){
    if(!transmitEnabled())return false;
    return sendReadRequest(radio,port,decoder,id,false);
  }
  bool sendReadRequest(Cc1101Radio& radio,HardwareSerial& port,
                ramses::FrameDecoder& decoder,uint8_t id,bool experimental){
    if(experimental) {
      if(id!=ot_single::DATA_ID || !experimentalOneShotReady())return false;
    } else if(!transmitEnabled())return false;
    uint8_t frame[16],wire[44];size_t used=0;
    if(!ramses_read::makeRequest(config_->otSource,config_->otBridge,id,frame,used))
      return false;
    const size_t wireLen=ramses_read::encodeUART(frame,used,wire,sizeof(wire));
    if(!wireLen || wireLen!=44)return false;
    // CC1101 owns the asynchronous serial GDO0 input while transmitting.
    // No arbitrary RF packets are accepted from HTTP or MQTT.
    if(!radio.beginReadRequestTx(experimental))return false;
    const size_t written=port.write(wire,wireLen);
    port.flush();
    delayMicroseconds(300);
    const bool rxOk=radio.endReadRequestTx();
    while(port.available())port.read();
    decoder.reset(); // TX/RX mode change may leave a partial RX frame.
    return written==wireLen && rxOk;
  }
};
