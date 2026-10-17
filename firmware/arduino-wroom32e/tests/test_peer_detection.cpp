#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../RamsesGateway/RamsesPeerDetection.h"
#include "../RamsesGateway/RamsesReadRequest.h"
int main(){
  constexpr uint32_t BRIDGE=48994, CONTROLLER=245914;
  ramses::Message m{};
  m.messageType=3; m.addrMask=3; m.opcode=0x3EF0; m.length=9;
  ramses_read::putAddress(m.addresses[0],10,BRIDGE);
  ramses_read::putAddress(m.addresses[1],1,CONTROLLER);
  assert(ramses_peer::isBridgeSender(m,BRIDGE));
  assert(ramses_peer::controllerFromBridgeReply(m,BRIDGE)==CONTROLLER);
  assert(ramses_peer::controllerLinkedToBridge(m,BRIDGE)==CONTROLLER);
  assert(ramses_peer::controllerLinkedToBridge(m,BRIDGE+1)==0);
  // A 1FD4 self-addressed information frame is not controller evidence.
  m.opcode=0x1FD4;
  assert(ramses_peer::controllerLinkedToBridge(m,BRIDGE)==0);
  m.opcode=0x3EF0;
  m.messageType=1;
  assert(ramses_peer::controllerFromBridgeReply(m,BRIDGE)==0);
  m.messageType=3;
  ramses_read::putAddress(m.addresses[1],18,CONTROLLER);
  assert(ramses_peer::controllerLinkedToBridge(m,BRIDGE)==0);
  ramses_read::putAddress(m.addresses[1],1,CONTROLLER);
  m.addrMask=5;
  assert(ramses_peer::controllerLinkedToBridge(m,BRIDGE)==0);
  // A request addressed to the configured R8810A remains sufficient.
  m={};
  m.messageType=0; m.addrMask=3; m.opcode=0x3EF0;
  ramses_read::putAddress(m.addresses[0],1,CONTROLLER);
  ramses_read::putAddress(m.addresses[1],10,BRIDGE);
  assert(ramses_peer::isControllerRequestToBridge(m,BRIDGE));
  assert(ramses_peer::controllerLinkedToBridge(m,BRIDGE)==CONTROLLER);
  assert(ramses_peer::controllerLinkedToBridge(m,BRIDGE+1)==0);
  // Neither an untyped gateway sender nor an unrelated 10: bridge
  // can be mistaken for the real Evohome heating controller.
  ramses_read::putAddress(m.addresses[0],18,CONTROLLER);
  assert(ramses_peer::controllerLinkedToBridge(m,BRIDGE)==0);
  ramses_read::putAddress(m.addresses[0],1,CONTROLLER);
  ramses_read::putAddress(m.addresses[1],10,BRIDGE+1);
  assert(ramses_peer::controllerLinkedToBridge(m,BRIDGE)==0);
  // Codex P2: a reply to a pre-reboot virtual 01: is only provisional.
  ramses_peer::ExistingController observed{};
  ramses::Message virtualReply{};
  virtualReply.messageType=3; virtualReply.addrMask=3;
  virtualReply.opcode=0x3EF0;
  ramses_read::putAddress(virtualReply.addresses[0],10,BRIDGE);
  ramses_read::putAddress(virtualReply.addresses[1],1,160001);
  observed.note(virtualReply,BRIDGE);
  assert(observed.id==160001 && !observed.directRequestSeen);
  // The real Evohome 01:245914 emits its ordinary RQ and must prevail.
  ramses::Message realRequest{};
  realRequest.messageType=0;realRequest.addrMask=3;
  realRequest.opcode=0x3EF0;
  ramses_read::putAddress(realRequest.addresses[0],1,CONTROLLER);
  ramses_read::putAddress(realRequest.addresses[1],10,BRIDGE);
  observed.note(realRequest,BRIDGE);
  assert(observed.id==CONTROLLER && observed.directRequestSeen);
  // No later RP or unrelated bridge can displace direct controller evidence.
  observed.note(virtualReply,BRIDGE);
  observed.note(virtualReply,BRIDGE+1);
  assert(observed.id==CONTROLLER && observed.directRequestSeen);
  // A later different 01: request cannot mutate the first real controller.
  ramses_read::putAddress(realRequest.addresses[0],1,160001);
  observed.note(realRequest,BRIDGE);
  assert(observed.id==CONTROLLER);
  // Negative: an unrelated 1FD4 information packet yields no candidate.
  ramses_peer::ExistingController another{};
  virtualReply.opcode=0x1FD4;
  another.note(virtualReply,BRIDGE);
  assert(another.id==0 && !another.directRequestSeen);
  // P2: the maximum 18-bit bridge ID is allowed by gateway configuration.
  ramses::Message maxBridge{};
  maxBridge.messageType=3;maxBridge.addrMask=3;maxBridge.opcode=0x3EF0;
  ramses_read::putAddress(maxBridge.addresses[0],10,262143);
  ramses_read::putAddress(maxBridge.addresses[1],1,CONTROLLER);
  assert(ramses_peer::isBridgeSender(maxBridge,262143));
  assert(ramses_peer::controllerFromBridgeReply(maxBridge,262143)==CONTROLLER);
  assert(ramses_peer::controllerLinkedToBridge(maxBridge,262143)==CONTROLLER);
  ramses_read::putAddress(maxBridge.addresses[0],1,CONTROLLER);
  ramses_read::putAddress(maxBridge.addresses[1],10,262143);
  maxBridge.messageType=0;
  assert(ramses_peer::isControllerRequestToBridge(maxBridge,262143));
  assert(ramses_peer::controllerLinkedToBridge(maxBridge,262143)==CONTROLLER);
  assert(ramses_peer::controllerLinkedToBridge(maxBridge,262142)==0);
  assert(!ramses_peer::isBridgeSender(maxBridge,0));
  printf("RAMSES bridge-to-controller passive 3EF0 inference PASS\n");
  return 0;
}
