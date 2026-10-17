// Passive discovery of the existing Evohome controller related to an R8810A.
// Never authorizes RF transmission by itself; operate on checksum-validated
// RAMSES messages only. Header-only to allow host-native regression testing.
#pragma once
#include <stdint.h>
#include "RamsesCodec.h"

namespace ramses_peer {
inline uint8_t type(const uint8_t* address) {
  return static_cast<uint8_t>(address[0] >> 2);
}
inline uint32_t id(const uint8_t* address) {
  return (static_cast<uint32_t>(address[0] & 3u) << 16) |
         (static_cast<uint32_t>(address[1]) << 8) | address[2];
}
inline bool isBridgeSender(const ramses::Message& message,uint32_t bridge) {
  return bridge>0 && bridge<262143 && (message.addrMask&1u) &&
         type(message.addresses[0])==10 &&
         id(message.addresses[0])==bridge;
}
inline bool isControllerRequestToBridge(const ramses::Message& message,
                                       uint32_t bridge) {
  return bridge>0 && bridge<262143 && (message.addrMask&3u)==3u &&
         type(message.addresses[0])==1 &&
         type(message.addresses[1])==10 &&
         id(message.addresses[1])==bridge;
}
inline uint32_t controllerFromBridgeReply(const ramses::Message& message,
                                          uint32_t bridge) {
  // 3EF0 is normal OpenTherm-bridge state traffic. A checksum-valid RP from
  // THIS bridge to a 01: controller is evidence of that controller even when
  // its original RQ was missed by the receiver.
  if(!isBridgeSender(message,bridge) ||
     (message.addrMask&3u)!=3u ||
     message.messageType!=3 || message.opcode!=0x3EF0 ||
     type(message.addresses[1])!=1)return 0;
  const uint32_t controller=id(message.addresses[1]);
  return controller>0 && controller<262143 ? controller : 0;
}
inline uint32_t controllerLinkedToBridge(const ramses::Message& message,
                                         uint32_t bridge) {
  if(isControllerRequestToBridge(message,bridge))
    return id(message.addresses[0]);
  return controllerFromBridgeReply(message,bridge);
}
} // namespace ramses_peer
