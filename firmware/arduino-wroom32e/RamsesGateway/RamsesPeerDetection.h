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
  return bridge>0 && bridge<=262143 && (message.addrMask&1u) &&
         type(message.addresses[0])==10 &&
         id(message.addresses[0])==bridge;
}
inline bool isControllerRequestToBridge(const ramses::Message& message,
                                       uint32_t bridge) {
  return bridge>0 && bridge<=262143 && (message.addrMask&3u)==3u &&
         type(message.addresses[0])==1 &&
         type(message.addresses[1])==10 &&
         id(message.addresses[1])==bridge;
}
// R8810A 3150 I heat-demand status uses addr3 (not addr2) for the real
// Evohome 01: controller. Observed passive traffic:
//  I --- 10:048994 --:------ 01:245914 3150 002 FC00
// Restrict inference to the configured bridge, exact 3150/status shape and
// the FC domain. Other devices' 3150 traffic must not authorize pairing.
inline bool isBridgeDemandToController(const ramses::Message& message,
                                       uint32_t bridge) {
  if(!isBridgeSender(message,bridge) ||
     message.messageType!=1 || message.opcode!=0x3150 ||
     message.addrMask!=5u || message.length!=2 ||
     message.payload[0]!=0xFC ||
     type(message.addresses[2])!=1)return false;
  const uint32_t controller=id(message.addresses[2]);
  return controller>0 && controller<262143;
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
  if(isBridgeDemandToController(message,bridge))
    return id(message.addresses[2]);
  return controllerFromBridgeReply(message,bridge);
}

// Reply-only observations are provisional. An explicitly observed controller
// request to the bridge is stronger evidence and replaces an earlier RP-only
// guess, but later replies cannot overwrite a directly observed controller.
struct ExistingController {
  uint32_t id=0;
  bool directRequestSeen=false;
  bool bridgeDemandSeen=false;
  void note(const ramses::Message& message,uint32_t bridge) {
    const bool direct=isControllerRequestToBridge(message,bridge);
    const bool bridgeDemand=isBridgeDemandToController(message,bridge);
    const uint32_t candidate=controllerLinkedToBridge(message,bridge);
    if(!candidate || candidate>=262143)return;
    if(direct && !directRequestSeen) {
      id=candidate;
      directRequestSeen=true;
    } else if(bridgeDemand && !directRequestSeen && !bridgeDemandSeen) {
      // Explicit bridge->controller 3150 outranks a provisional 3EF0 RP.
      id=candidate;
      bridgeDemandSeen=true;
    } else if(!id) {
      id=candidate;
    }
  }
};
} // namespace ramses_peer
