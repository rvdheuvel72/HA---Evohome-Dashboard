// Passive RF diagnostics and CC1101 signal conversion (host-testable).
// No RF transmit, network or Arduino dependency.
#pragma once
#include <stdint.h>
#include "RamsesCodec.h"

namespace ramses_rx {
inline int16_t rssiDbm(uint8_t registerValue) {
  // CC1101 signed two's-complement RSSI: RSSI_dBm = RSSI/2 - 74dB.
  // Prior code incorrectly added +256 to negative RSSI register samples.
  return static_cast<int16_t>(static_cast<int8_t>(registerValue))/2 - 74;
}
inline uint8_t rssiMagnitude(uint8_t registerValue) {
  const int magnitude=-rssiDbm(registerValue);
  return static_cast<uint8_t>(magnitude>255?255:(magnitude<0?0:magnitude));
}
inline uint8_t rfType(const uint8_t* a){return a[0]>>2;}
inline uint32_t rfId(const uint8_t* a){
  return (static_cast<uint32_t>(a[0]&3u)<<16) |
         (static_cast<uint32_t>(a[1])<<8) | a[2];
}
struct Counters {
  uint32_t checksum=0, invalidSymbol=0, malformed=0, oversized=0,
           incomplete=0, idleTimeout=0, formatFailure=0;
  uint32_t requestsToBridge=0, messagesFromBridge=0,
           bridge3ef0=0, bridge3220=0;
  uint32_t lastBridgeMs=0, lastRequestMs=0, lastRejectMs=0;
  ramses::RejectReason lastReject=ramses::NO_REJECTION;
  void rejection(ramses::RejectReason reason,uint32_t now) {
    if(reason==ramses::NO_REJECTION)return;
    lastReject=reason;lastRejectMs=now;
    switch(reason){
      case ramses::BAD_CHECKSUM:checksum++;break;
      case ramses::INVALID_SYMBOL:invalidSymbol++;break;
      case ramses::MALFORMED_FRAME:malformed++;break;
      case ramses::OVERSIZED_FRAME:oversized++;break;
      case ramses::INCOMPLETE_FRAME:incomplete++;break;
      case ramses::IDLE_TIMEOUT:idleTimeout++;break;
      default:break;
    }
  }
  void packet(const ramses::Message& msg,uint32_t bridgeId,uint32_t now) {
    if(bridgeId==0)return;
    if((msg.addrMask&1u) && rfType(msg.addresses[0])==10 &&
       rfId(msg.addresses[0])==bridgeId) {
      messagesFromBridge++;lastBridgeMs=now;
      if(msg.opcode==0x3EF0)bridge3ef0++;
      if(msg.opcode==0x3220)bridge3220++;
    }
    if(msg.messageType==0 && (msg.addrMask&2u) &&
       rfType(msg.addresses[1])==10 && rfId(msg.addresses[1])==bridgeId) {
      requestsToBridge++;lastRequestMs=now;
    }
  }
};
} // namespace ramses_rx
