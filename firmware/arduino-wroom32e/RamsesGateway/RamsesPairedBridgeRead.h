// One deliberately authorized read-only OpenTherm request to the user's own
// R8810A, using the observed original Evohome 01: controller identity.
// No binding, writes, retries, scans or automatic RF output.
#pragma once
#include <stdint.h>
#include "RamsesCodec.h"
#include "RamsesReadRequest.h"
#include "OpenThermReadOnly.h"
namespace ramses_bridge_read {
constexpr uint32_t COOLDOWN_MS=60000u;
constexpr uint32_t RESPONSE_TIMEOUT_MS=1800u;
inline bool supportedId(uint8_t id) {
  switch(id) {
    case 5:case 17:case 18:case 19:case 25:case 26:case 27:
    case 28:case 33:case 57:return true;
    default:return false;
  }
}
enum State : uint8_t {
  IDLE,QUEUED,WAITING,READ_ACK,DATA_INVALID,UNKNOWN_ID,TIMEOUT,TX_FAILED
};
inline const char* label(State state) {
  switch(state) {
    case IDLE:return "not_tested";
    case QUEUED:return "queued";
    case WAITING:return "waiting_for_bridge";
    case READ_ACK:return "read_ack";
    case DATA_INVALID:return "data_invalid";
    case UNKNOWN_ID:return "unknown_data_id";
    case TIMEOUT:return "timeout";
    case TX_FAILED:return "tx_failed";
  }
  return "unknown";
}
struct Probe {
  State state=IDLE;
  uint32_t controllerId=0,bridgeId=0,lastAttemptMs=0,deadlineMs=0;
  uint8_t dataId=25;
  uint16_t raw=0;
  bool attempted=false;
  bool busy()const{return state==QUEUED || state==WAITING;}
  uint32_t cooldown(uint32_t now)const {
    if(!attempted)return 0;
    const uint32_t elapsed=now-lastAttemptMs;
    return elapsed>=COOLDOWN_MS?0:COOLDOWN_MS-elapsed;
  }
  bool request(uint32_t now,uint32_t controller,uint32_t bridge,uint8_t id) {
    if(busy()||cooldown(now)||!supportedId(id)||
       !controller||controller>=262143||!bridge||bridge>=262143)return false;
    controllerId=controller;bridgeId=bridge;dataId=id;raw=0;state=QUEUED;
    return true;
  }
  bool begin(uint32_t now) {
    if(state!=QUEUED)return false;
    attempted=true;lastAttemptMs=now;deadlineMs=now+RESPONSE_TIMEOUT_MS;
    state=WAITING;return true;
  }
  void sent(bool ok){if(state==WAITING && !ok)state=TX_FAILED;}
  void tick(uint32_t now){
    if(state==WAITING && static_cast<int32_t>(now-deadlineMs)>=0)state=TIMEOUT;
  }
  bool observe(const ramses::Message& m,uint32_t now) {
    tick(now);
    if(state!=WAITING || m.messageType!=3 || m.opcode!=0x3220 ||
       (m.addrMask&3u)!=3u ||
       ramses_peer_type(m.addresses[0])!=10 ||
       ramses_peer_id(m.addresses[0])!=bridgeId ||
       ramses_peer_type(m.addresses[1])!=1 ||
       ramses_peer_id(m.addresses[1])!=controllerId)return false;
    uint8_t receivedId=0;ot_readonly::Result result=ot_readonly::NOT_TESTED;
    uint16_t value=0;
    if(!ot_readonly::response(m.payload,m.length,receivedId,result,value)||
       receivedId!=dataId)return false;
    switch(result) {
      case ot_readonly::READ_ACK:state=READ_ACK;break;
      case ot_readonly::DATA_INVALID:state=DATA_INVALID;break;
      case ot_readonly::UNKNOWN_ID:state=UNKNOWN_ID;break;
      default:return false;
    }
    raw=value;
    return true;
  }
  static uint8_t ramses_peer_type(const uint8_t* a){return a[0]>>2;}
  static uint32_t ramses_peer_id(const uint8_t* a){
    return (static_cast<uint32_t>(a[0]&3u)<<16) |
           (static_cast<uint32_t>(a[1])<<8) | a[2];
  }
};
inline bool makeRead(uint32_t controller,uint32_t bridge,uint8_t id,
                     uint8_t out[16],size_t& used) {
  if(!supportedId(id))return false;
  return ramses_read::makeRequestTyped(1,controller,bridge,id,out,used);
}
} // namespace ramses_bridge_read
