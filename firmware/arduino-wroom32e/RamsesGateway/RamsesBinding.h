// Experimental RAMSES-II R8810A additional-controller binding.
// Device-side only, no Arduino dependencies. No automatic retries or writes.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "RamsesCodec.h"
#include "RamsesReadRequest.h"
#include "OpenThermReadOnly.h"
namespace ramses_bind {
enum State : uint8_t {
  IDLE, OFFER_QUEUED, WAIT_RESPONSE, CONFIRM_QUEUED,
  VERIFY_QUEUED, WAIT_VERIFY, VERIFIED, UNVERIFIED, TIMED_OUT, TX_FAILED, ID_CONFLICT
};
inline const char* name(State s) {
  switch(s) {
    case IDLE:return "not_started";
    case OFFER_QUEUED:return "offer_queued";
    case WAIT_RESPONSE:return "waiting_for_1fc9_response";
    case CONFIRM_QUEUED:return "confirmation_queued";
    case VERIFY_QUEUED:return "verification_queued";
    case WAIT_VERIFY:return "waiting_for_3220_reply";
    case VERIFIED:return "verified_read_reply";
    case UNVERIFIED:return "confirmation_sent_unverified";
    case TIMED_OUT:return "no_binding_response";
    case TX_FAILED:return "local_tx_failed";
    case ID_CONFLICT:return "virtual_id_conflict";
  }
  return "unknown";
}
constexpr uint32_t RESPONSE_TIMEOUT_MS=15000;
constexpr uint32_t VERIFY_TIMEOUT_MS=2500;
constexpr uint32_t COOLDOWN_MS=60000;
inline uint8_t type(const uint8_t* a) {return a[0]>>2;}
inline uint32_t id(const uint8_t* a) {
  return (static_cast<uint32_t>(a[0]&3u)<<16) |
     (static_cast<uint32_t>(a[1])<<8)|a[2];
}
inline bool candidate(uint32_t virtualId,uint32_t bridgeId,uint32_t actualController) {
  return virtualId>0 && virtualId<262143 &&
    bridgeId>0 && bridgeId<=262143 && virtualId!=actualController;
}
inline bool makeOffer(uint32_t virtualId,uint32_t bridgeId,
                      uint8_t out[64],size_t& used) {
  used=0;
  if(!out || !candidate(virtualId,bridgeId,0))return false;
  // Observed actuator binding pattern for a 01: controller:
  // 0008 + 3B00 + 1FC9 capability triplets; R8810A acceptance is experimental.
  // 'I' from 01:X, unused addr1, 01:X as third address (header 0x18).
  out[0]=0x18;
  ramses_read::putAddress(out+1,1,virtualId);
  ramses_read::putAddress(out+4,1,virtualId);
  out[7]=0x1F;out[8]=0xC9;out[9]=18;
  const uint8_t zones[3]={0x00,0xFC,0x00};
  const uint16_t opcodes[3]={0x0008,0x3B00,0x1FC9};
  for(unsigned i=0;i<3;i++) {
    const size_t p=10+i*6;
    out[p]=zones[i];
    out[p+1]=static_cast<uint8_t>(opcodes[i]>>8);
    out[p+2]=static_cast<uint8_t>(opcodes[i]);
    ramses_read::putAddress(out+p+3,1,virtualId);
  }
  uint8_t sum=0;
  for(size_t i=0;i<28;i++)sum=static_cast<uint8_t>(sum+out[i]);
  out[28]=static_cast<uint8_t>(0u-sum);
  used=29;
  return true;
}
inline bool makeConfirmation(uint32_t virtualId,uint32_t bridgeId,
                            uint8_t out[64],size_t& used) {
  used=0;
  if(!out || !candidate(virtualId,bridgeId,0))return false;
  // 'I' from 01:X to 10:Y; 1FC9 006 00FFFF[01:X]
  out[0]=0x1C;
  ramses_read::putAddress(out+1,1,virtualId);
  ramses_read::putAddress(out+4,10,bridgeId);
  out[7]=0x1F;out[8]=0xC9;out[9]=6;
  out[10]=0;out[11]=0xFF;out[12]=0xFF;
  ramses_read::putAddress(out+13,1,virtualId);
  uint8_t sum=0;
  for(size_t i=0;i<16;i++)sum=static_cast<uint8_t>(sum+out[i]);
  out[16]=static_cast<uint8_t>(0u-sum);
  used=17;
  return true;
}
inline bool makeVerifyRead(uint32_t virtualId,uint32_t bridgeId,
                           uint8_t out[16],size_t& used) {
  return ramses_read::makeRequestTyped(1,virtualId,bridgeId,25,out,used);
}
struct Session {
  State state=IDLE;
  uint32_t virtualId=0,bridgeId=0,lastAttempt=0,deadline=0;
  bool attempted=false,acceptedOffer=false,confirmationSent=false;
  bool verifiedReadAck=false,virtualIdCollision=false;
  uint16_t lastRaw=0;
  bool busy() const {
    return state==OFFER_QUEUED||state==WAIT_RESPONSE||
      state==CONFIRM_QUEUED||state==VERIFY_QUEUED||state==WAIT_VERIFY;
  }
  uint32_t cooldown(uint32_t now) const {
    if(!attempted)return 0;
    uint32_t age=now-lastAttempt;
    return age>=COOLDOWN_MS?0:COOLDOWN_MS-age;
  }
  bool start(uint32_t now,uint32_t candidateId,uint32_t bridge,uint32_t existingController) {
    if(busy()||cooldown(now)||!candidate(candidateId,bridge,existingController))
      return false;
    virtualId=candidateId;bridgeId=bridge;lastAttempt=now;
    attempted=true;acceptedOffer=false;confirmationSent=false;
    verifiedReadAck=false;virtualIdCollision=false;lastRaw=0;
    state=OFFER_QUEUED;deadline=0;
    return true;
  }
  void offered(bool sent,uint32_t now) {
    if(state!=OFFER_QUEUED)return;
    state=sent?WAIT_RESPONSE:TX_FAILED;
    deadline=now+RESPONSE_TIMEOUT_MS;
  }
  bool observe(const ramses::Message& m,uint32_t now) {
    // RX can be drained before the next radio-task tick. Never accept a
    // binding or verification reply at/after its deadline, even when
    // the radio task was starved. Signed subtraction also handles millis()
    // rollover for the short (15s / 2.5s) bounded wait windows.
    tick(now);
    if((!busy() && state!=VERIFIED) || !(m.addrMask&1))return false;
    if(type(m.addresses[0])==1 &&
       id(m.addresses[0])==virtualId) {
      virtualIdCollision=true;state=ID_CONFLICT;return false;
    }
    if(state==WAIT_RESPONSE && m.opcode==0x1FC9 &&
       m.messageType==2 && (m.addrMask&3u)==3u &&
       type(m.addresses[0])==10 && id(m.addresses[0])==bridgeId &&
       type(m.addresses[1])==1 && id(m.addresses[1])==virtualId &&
       m.length==6 && m.payload[0]==0 &&
       m.payload[1]==0x3e && m.payload[2]==0xf0 &&
       type(m.payload+3)==10 && id(m.payload+3)==bridgeId) {
      acceptedOffer=true;state=CONFIRM_QUEUED;return true;
    }
    if(state==WAIT_VERIFY && m.opcode==0x3220 &&
       m.messageType==3 && (m.addrMask&3u)==3u &&
       type(m.addresses[0])==10 && id(m.addresses[0])==bridgeId &&
       type(m.addresses[1])==1 && id(m.addresses[1])==virtualId) {
      uint8_t dataId=0; uint16_t raw=0;
      ot_readonly::Result result=ot_readonly::NOT_TESTED;
      if(ot_readonly::response(m.payload,m.length,dataId,result,raw) &&
         dataId==25) {
        lastRaw=raw;
        verifiedReadAck=result==ot_readonly::READ_ACK;
        state=VERIFIED;
        return true;
      }
    }
    return false;
  }
  void confirmed(bool sent) {
    if(state!=CONFIRM_QUEUED)return;
    confirmationSent=sent;
    state=sent?VERIFY_QUEUED:TX_FAILED;
  }
  void verifiedRequestSent(bool sent,uint32_t now) {
    if(state!=VERIFY_QUEUED)return;
    state=sent?WAIT_VERIFY:UNVERIFIED;
    deadline=now+VERIFY_TIMEOUT_MS;
  }
  void tick(uint32_t now) {
    if((state==WAIT_RESPONSE||state==WAIT_VERIFY) &&
       static_cast<int32_t>(now-deadline)>=0)
      state=(state==WAIT_RESPONSE)?TIMED_OUT:UNVERIFIED;
  }
};
} // namespace ramses_bind
