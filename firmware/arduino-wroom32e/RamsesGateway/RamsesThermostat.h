// Virtual RAMSES-II room thermostat (12:) alongside existing OpenTherm 01: controller.
// Deterministic, bounded and Arduino-independent: no boiler writes or automatic pairing.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "RamsesCodec.h"
#include "RamsesReadRequest.h"
namespace ramses_thermostat {
enum State : uint8_t {
  IDLE, OFFER_QUEUED, WAIT_ACCEPT, CONFIRM_QUEUED,
  PAIRED, NO_RESPONSE, TX_FAILED, ID_CONFLICT
};
inline const char* stateName(State s) {
  switch(s) {
    case IDLE:return "not_started";
    case OFFER_QUEUED:return "offer_queued";
    case WAIT_ACCEPT:return "waiting_for_controller_accept";
    case CONFIRM_QUEUED:return "confirmation_queued";
    case PAIRED:return "controller_accepted_confirmation_sent";
    case NO_RESPONSE:return "no_controller_accept";
    case TX_FAILED:return "local_tx_failed";
    case ID_CONFLICT:return "virtual_thermostat_id_conflict";
  }
  return "unknown";
}
constexpr uint32_t ACCEPT_TIMEOUT_MS=15000;
constexpr uint32_t COOLDOWN_MS=60000;
constexpr uint32_t REPORT_INTERVAL_MS=300000;
constexpr uint32_t TEMPERATURE_TTL_MS=900000;
inline uint8_t type(const uint8_t* a){return a[0]>>2;}
inline uint32_t id(const uint8_t* a) {
  return (static_cast<uint32_t>(a[0]&3u)<<16) |
    (static_cast<uint32_t>(a[1])<<8)|a[2];
}
inline bool validIdentity(uint32_t t,uint32_t controller) {
  return t>0 && t<262143 && controller>0 && controller<262143;
}
inline bool pack(const uint8_t* content,size_t body,uint8_t* out,
                 size_t cap,size_t& used) {
  used=0;
  if(!content || !out || body+1>cap || body>=64)return false;
  memcpy(out,content,body);
  uint8_t sum=0;
  for(size_t i=0;i<body;i++)sum=static_cast<uint8_t>(sum+out[i]);
  out[body]=static_cast<uint8_t>(0u-sum);
  used=body+1;
  return true;
}
// Captured 12:010740 thermostat offer:
// 1FC9 024 0023093029F4 0030C93029F4 0000083029F4 001FC93029F4
inline bool makeOffer(uint32_t thermostatId,uint32_t controllerId,
                      uint8_t out[64],size_t& used) {
  used=0;
  if(!validIdentity(thermostatId,controllerId)||!out)return false;
  uint8_t body[34]={};
  body[0]=0x18; // I from 12:virtual, --:------, 12:virtual
  ramses_read::putAddress(body+1,12,thermostatId);
  ramses_read::putAddress(body+4,12,thermostatId);
  body[7]=0x1F;body[8]=0xC9;body[9]=24;
  const uint16_t codes[4]={0x2309,0x30C9,0x0008,0x1FC9};
  for(size_t i=0;i<4;i++){
    const size_t n=10+i*6;
    body[n]=0;
    body[n+1]=static_cast<uint8_t>(codes[i]>>8);
    body[n+2]=static_cast<uint8_t>(codes[i]);
    ramses_read::putAddress(body+n+3,12,thermostatId);
  }
  return pack(body,sizeof(body),out,64,used);
}
// W controller -> 12:thermostat 1FC9 006 <zone>2309<controller01>
inline bool controllerAccept(const ramses::Message& m,
                             uint32_t virtualId,uint32_t controllerId,
                             uint8_t& zone) {
  if(m.messageType!=2 || m.opcode!=0x1FC9 ||
     (m.addrMask&3u)!=3u || m.length!=6 ||
     type(m.addresses[0])!=1 || id(m.addresses[0])!=controllerId ||
     type(m.addresses[1])!=12 || id(m.addresses[1])!=virtualId ||
     m.payload[0]>11 || m.payload[1]!=0x23 || m.payload[2]!=0x09 ||
     type(m.payload+3)!=1 || id(m.payload+3)!=controllerId)return false;
  zone=m.payload[0];return true;
}
// Actual DTS92-style type-12 thermostat trace from ramses_rf code_1fc9.log:
// Controller W 1FC9 006 08230906368E (assigned zone 08), BUT the 12:
// sensor confirms using index 00: I 1FC9 006 0023093029F4.
// Unlike a 34: T87RF thermostat, a 12: DTS92 does not echo zone in confirm.
inline bool makeConfirmation(uint32_t thermostatId,uint32_t controllerId,
                             uint8_t zone,uint8_t out[64],size_t& used) {
  used=0;
  if(!validIdentity(thermostatId,controllerId)||zone>11||!out)return false;
  uint8_t body[16]={};
  body[0]=0x1C; // I, two explicit addresses
  ramses_read::putAddress(body+1,12,thermostatId);
  ramses_read::putAddress(body+4,1,controllerId);
  body[7]=0x1F;body[8]=0xC9;body[9]=6;
  body[10]=0x00;body[11]=0x23;body[12]=0x09;
  ramses_read::putAddress(body+13,12,thermostatId);
  return pack(body,sizeof(body),out,64,used);
}
// Real 12: DTS92 traffic (ramses_rf code_30c9.log) announces its
// temperature using a self-addressed I with index 00, NOT a directed
// zone-indexed I to the controller. Evohome internally maps this sensor's
// identity to the assigned zone from the earlier 1FC9 acceptance.
// Only a valid, fresh external temperature may be transmitted.
inline bool makeTemperature(uint32_t thermostatId,uint32_t controllerId,
                            uint8_t zone,int32_t centiC,
                            uint8_t out[64],size_t& used) {
  used=0;
  if(!validIdentity(thermostatId,controllerId)||zone>11||
     centiC < -4000 || centiC > 8500 || !out)return false;
  uint8_t body[13]={};
  body[0]=0x18; // I --- 12:virtual --:------ 12:virtual
  ramses_read::putAddress(body+1,12,thermostatId);
  ramses_read::putAddress(body+4,12,thermostatId);
  body[7]=0x30;body[8]=0xC9;body[9]=3;body[10]=0x00;
  const uint16_t encoded=static_cast<uint16_t>(static_cast<int16_t>(centiC));
  body[11]=static_cast<uint8_t>(encoded>>8);body[12]=static_cast<uint8_t>(encoded);
  return pack(body,sizeof(body),out,64,used);
}
// Timestamped MQTT wire value: <Unix epoch seconds>:<signed centi-Celsius>.
// PubSubClient hides MQTT RETAIN in its callback, so a plain integer cannot
// safely distinguish a newly sampled value from a retained replay.
inline bool mqttStampedTemperature(const char* input,size_t length,
                                   uint32_t nowEpoch,int32_t& centi,
                                   uint32_t& ageMs) {
  centi=0;ageMs=0;
  if(!input || length<12 || length>18 || nowEpoch<1600000000u)return false;
  size_t n=0;uint64_t event=0;
  for(;n<length && input[n]!=':';n++) {
    if(input[n]<'0'||input[n]>'9'||n>=11)return false;
    event=event*10u+static_cast<unsigned>(input[n]-'0');
  }
  if(n<10 || n>=length || input[n]!=':' || event>UINT32_MAX)return false;
  ++n;
  bool negative=false;
  if(n<length && input[n]=='-'){negative=true;n++;}
  if(n>=length)return false;
  int32_t absValue=0;
  for(;n<length;n++){
    if(input[n]<'0'||input[n]>'9')return false;
    absValue=absValue*10+static_cast<int32_t>(input[n]-'0');
    if(absValue>8500)return false;
  }
  centi=negative?-absValue:absValue;
  if(centi<-4000||centi>8500)return false;
  if(event>static_cast<uint64_t>(nowEpoch)+30u)return false;
  const uint32_t age=event>nowEpoch?0u:nowEpoch-static_cast<uint32_t>(event);
  if(age>=TEMPERATURE_TTL_MS/1000u)return false;
  ageMs=age*1000u;
  return true;
}

struct Session {
  State state=IDLE;
  uint32_t thermostatId=0,controllerId=0,lastAttempt=0,deadline=0;
  uint32_t lastTempAt=0,lastReportAt=0;
  int32_t tempCenti=0;
  uint8_t zone=0xff;
  bool attempted=false,accepted=false,confirmed=false,collision=false;
  bool hasTemp=false,hasReported=false;
  // Local stop/forget only: does NOT send a RAMSES unbind packet and
  // cannot erase an association stored by the physical Evohome device.
  // Preserve the existing cooldown to prevent instant RF re-pair spam.
  void forget() {
    const bool wasAttempted=attempted;
    const uint32_t attemptedAt=lastAttempt;
    *this=Session{};
    attempted=wasAttempted;
    lastAttempt=attemptedAt;
  }
  bool busy() const {
    return state==OFFER_QUEUED||state==WAIT_ACCEPT||state==CONFIRM_QUEUED;
  }
  uint32_t cooldown(uint32_t now)const{
    if(!attempted)return 0;
    const uint32_t age=now-lastAttempt;
    return age>=COOLDOWN_MS?0:COOLDOWN_MS-age;
  }
  bool request(uint32_t now,uint32_t deviceId,uint32_t controller) {
    if(busy() || cooldown(now) || !validIdentity(deviceId,controller))
      return false;
    thermostatId=deviceId;controllerId=controller;
    lastAttempt=now;attempted=true;
    zone=0xff;accepted=false;confirmed=false;collision=false;
    hasReported=false;hasTemp=false;state=OFFER_QUEUED;
    return true;
  }
  void offered(bool sent,uint32_t now){
    if(state!=OFFER_QUEUED)return;
    state=sent?WAIT_ACCEPT:TX_FAILED;
    deadline=now+ACCEPT_TIMEOUT_MS;
  }
  void tick(uint32_t now){
    if(state==WAIT_ACCEPT && static_cast<int32_t>(now-deadline)>=0)
      state=NO_RESPONSE;
    // A stale sample must NEVER revive after millis() rolls over (49.7d).
    // Expiration is latched until a new explicit sensor update arrives.
    if(hasTemp && static_cast<uint32_t>(now-lastTempAt)>=TEMPERATURE_TTL_MS)
      hasTemp=false;
  }
  bool observe(const ramses::Message& m,uint32_t now) {
    tick(now);
    if(!(m.addrMask&1u))return false;
    if(thermostatId && type(m.addresses[0])==12 &&
       id(m.addresses[0])==thermostatId) {
      collision=true;state=ID_CONFLICT;return false;
    }
    uint8_t acceptedZone=0xff;
    if(state==WAIT_ACCEPT && controllerAccept(m,thermostatId,controllerId,acceptedZone)){
      zone=acceptedZone;accepted=true;state=CONFIRM_QUEUED;return true;
    }
    return false;
  }
  void confirmationSent(bool sent) {
    if(state!=CONFIRM_QUEUED)return;
    confirmed=sent;state=sent?PAIRED:TX_FAILED;
  }
  bool setTemperature(int32_t centiC,uint32_t now){
    if(centiC < -4000 || centiC > 8500)return false;
    tempCenti=centiC;lastTempAt=now;hasTemp=true;
    return true;
  }
  bool temperatureFresh(uint32_t now) const {
    return hasTemp && static_cast<uint32_t>(now-lastTempAt)<TEMPERATURE_TTL_MS;
  }
  bool reportDue(uint32_t now) const {
    return state==PAIRED && confirmed && !collision &&
      temperatureFresh(now) &&
      (!hasReported || static_cast<uint32_t>(now-lastReportAt)>=REPORT_INTERVAL_MS);
  }
  void reported(bool sent,uint32_t now) {
    if(sent){hasReported=true;lastReportAt=now;}
    // Never spin on hardware TX faults.
    else {hasReported=true;lastReportAt=now;}
  }
};
} // namespace ramses_thermostat
