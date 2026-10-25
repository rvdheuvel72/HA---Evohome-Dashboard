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
  // Real controller -> OpenTherm bridge 1FC9 offer, NOT a BDR relay
  // heating-valve offer. Independent upstream capture:
  // ramses-rf/ramses_rf tests/tests_rf/data_driven/parsers/code_1fc9.log
  // 2020-11-23 14:35:23: 01:078710 -> 10:067219
  //   1FC9 024 FC|0008 FC|3150 FB|3150 FC|1FC9 (each has controller ID).
  // No additional-controller coexistence is established by this capture.
  // I from 01:X, unset second address, 01:X as third address.
  static constexpr uint8_t DOMAINS[4]={0xFC,0xFC,0xFB,0xFC};
  static constexpr uint16_t CODES[4]={0x0008,0x3150,0x3150,0x1FC9};
  static constexpr size_t ENTRY_COUNT=4;
  static constexpr size_t PAYLOAD_LEN=ENTRY_COUNT*6;
  static constexpr size_t HEADER_LEN=10;
  out[0]=0x18;
  ramses_read::putAddress(out+1,1,virtualId);
  ramses_read::putAddress(out+4,1,virtualId);
  out[7]=0x1F;out[8]=0xC9;out[9]=static_cast<uint8_t>(PAYLOAD_LEN);
  for(size_t i=0;i<ENTRY_COUNT;i++) {
    const size_t p=HEADER_LEN+i*6;
    out[p]=DOMAINS[i];
    out[p+1]=static_cast<uint8_t>(CODES[i]>>8);
    out[p+2]=static_cast<uint8_t>(CODES[i]);
    ramses_read::putAddress(out+p+3,1,virtualId);
  }
  uint8_t sum=0;
  for(size_t i=0;i<HEADER_LEN+PAYLOAD_LEN;i++)
    sum=static_cast<uint8_t>(sum+out[i]);
  out[HEADER_LEN+PAYLOAD_LEN]=static_cast<uint8_t>(0u-sum);
  used=HEADER_LEN+PAYLOAD_LEN+1;
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
  ot_readonly::Result verificationResult=ot_readonly::NOT_TESTED;
  uint16_t lastRaw=0;
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
    verifiedReadAck=false;virtualIdCollision=false;
    verificationResult=ot_readonly::NOT_TESTED;lastRaw=0;
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
        verificationResult=result;
        verifiedReadAck=result==ot_readonly::READ_ACK;
        // Data-Invalid and Unknown-DataId prove an addressed response,
        // NOT a successful OpenTherm ID25 read or verified 01: binding.
        // Fail closed instead of allowing later 01: requests from a
        // session whose verification did not receive a Read-Ack.
        state=verifiedReadAck?VERIFIED:UNVERIFIED;
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
