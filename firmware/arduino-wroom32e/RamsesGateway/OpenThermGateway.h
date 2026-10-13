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
#include "RamsesReadRequest.h"
#include "Cc1101Radio.h"
#include "RamsesCodec.h"

class OpenThermGateway {
 public:
  bool transmitEnabled() const {
    return GW_OT_ENABLE_RF_TX == 1 &&
        GW_OT_BRIDGE_ID > 0 && GW_OT_BRIDGE_ID <= 262143 &&
        GW_OT_SOURCE_ID > 0 && GW_OT_SOURCE_ID < 262143 &&
        GW_OT_SOURCE_ID != 730 &&
        radio_frequency::evohomeTxFrequency(GW_CC1101_FREQUENCY_KHZ,
                                              GW_CC1101_MODULE_BAND);
  }
  void begin() {
    Preferences p;
    if (p.begin("ot-probe",true)) {
      Persist stored = {};
      if (p.getBytesLength("profile")==sizeof(stored) &&
          p.getBytes("profile",&stored,sizeof(stored))==sizeof(stored) &&
          stored.magic==PROFILE_MAGIC &&
          stored.bridgeId==GW_OT_BRIDGE_ID &&
          stored.frequency==GW_CC1101_FREQUENCY_KHZ) {
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
       (GW_OT_BRIDGE_ID!=0 && rfId(msg.addresses[0])!=GW_OT_BRIDGE_ID) ||
       !ot_readonly::response(msg.payload,msg.length,id,r,raw))return false;
    // Only attribute scan results to our transaction when the reply
    // explicitly addresses the configured gateway RF identity.
    if((msg.addrMask&2) && rfType(msg.addresses[1])==18 &&
       rfId(msg.addresses[1])==GW_OT_SOURCE_ID) {
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
    if(start) {
      work_.reset(GW_OT_BRIDGE_ID,GW_OT_SCAN_ALL_256==1,now);
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
    if(GW_OT_AUTO_POLL==1 && !work_.active && work_.completed &&
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
      static_cast<unsigned long>(GW_OT_BRIDGE_ID),
      static_cast<unsigned long>(GW_CC1101_FREQUENCY_KHZ),
      static_cast<unsigned>(GW_CC1101_MODULE_BAND),
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
    server.sendContent("]}");
    server.sendContent("");
  }
 private:
  struct Persist {
    uint32_t magic,bridgeId,frequency;
    uint8_t states[256];
    uint16_t raw[256];
  };
  static constexpr uint32_t PROFILE_MAGIC=0x4f543034;
  portMUX_TYPE mutex_=portMUX_INITIALIZER_UNLOCKED;
  ot_scan::Scanner work_,published_;
  bool requested_=false;
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
    data.bridgeId=GW_OT_BRIDGE_ID;
    data.frequency=GW_CC1101_FREQUENCY_KHZ;
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
    uint8_t frame[16],wire[44];size_t used=0;
    if(!ramses_read::makeRequest(GW_OT_SOURCE_ID,GW_OT_BRIDGE_ID,id,frame,used))
      return false;
    const size_t wireLen=ramses_read::encodeUART(frame,used,wire,sizeof(wire));
    if(!wireLen || wireLen!=44)return false;
    // CC1101 owns the asynchronous serial GDO0 input while transmitting.
    // No arbitrary RF packets are accepted from HTTP or MQTT.
    if(!radio.beginReadRequestTx())return false;
    const size_t written=port.write(wire,wireLen);
    port.flush();
    delayMicroseconds(300);
    const bool rxOk=radio.endReadRequestTx();
    while(port.available())port.read();
    decoder.reset(); // TX/RX mode change may leave a partial RX frame.
    return written==wireLen && rxOk;
  }
};
