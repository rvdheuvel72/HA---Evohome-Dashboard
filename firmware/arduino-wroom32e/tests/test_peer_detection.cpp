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

  // Real R8810A 3150 I status (2026-10-18): controller is in addr3.
  // Unlike a 3EF0 RP there is no addr2, so pre-beta.16 never learned 01:.
  ramses::Message demand{};
  demand.messageType=1; demand.addrMask=5; demand.opcode=0x3150;
  demand.length=2; demand.payload[0]=0xFC; demand.payload[1]=0x00;
  ramses_read::putAddress(demand.addresses[0],10,BRIDGE);
  ramses_read::putAddress(demand.addresses[2],1,CONTROLLER);
  assert(ramses_peer::isBridgeDemandToController(demand,BRIDGE));
  assert(ramses_peer::controllerLinkedToBridge(demand,BRIDGE)==CONTROLLER);
  assert(ramses_peer::controllerLinkedToBridge(demand,BRIDGE+1)==0);
  // Identical 3150 from the heating valve 04:253821 is NOT bridge evidence.
  ramses_read::putAddress(demand.addresses[0],4,253821);
  assert(ramses_peer::controllerLinkedToBridge(demand,BRIDGE)==0);
  ramses_read::putAddress(demand.addresses[0],10,BRIDGE);
  demand.payload[0]=0x04;
  assert(ramses_peer::controllerLinkedToBridge(demand,BRIDGE)==0);
  demand.payload[0]=0xFC; demand.messageType=3;
  assert(ramses_peer::controllerLinkedToBridge(demand,BRIDGE)==0);
  demand.messageType=1; demand.opcode=0x2309;
  assert(ramses_peer::controllerLinkedToBridge(demand,BRIDGE)==0);
  demand.opcode=0x3150; demand.addrMask=3;
  assert(ramses_peer::controllerLinkedToBridge(demand,BRIDGE)==0);
  demand.addrMask=5; demand.length=1;
  assert(ramses_peer::controllerLinkedToBridge(demand,BRIDGE)==0);
  demand.length=2;
  ramses_read::putAddress(demand.addresses[2],18,CONTROLLER);
  assert(ramses_peer::controllerLinkedToBridge(demand,BRIDGE)==0);
  ramses_read::putAddress(demand.addresses[2],1,CONTROLLER);
  ramses_peer::ExistingController learnedFrom3150{};
  learnedFrom3150.note(demand,BRIDGE);
  assert(learnedFrom3150.id==CONTROLLER);
  assert(learnedFrom3150.bridgeDemandSeen && !learnedFrom3150.directRequestSeen);
  ramses_peer::ExistingController prefers3150{};
  ramses::Message provisionalReply{};
  provisionalReply.messageType=3; provisionalReply.addrMask=3;
  provisionalReply.opcode=0x3EF0;
  ramses_read::putAddress(provisionalReply.addresses[0],10,BRIDGE);
  ramses_read::putAddress(provisionalReply.addresses[1],1,160001);
  prefers3150.note(provisionalReply,BRIDGE);
  assert(prefers3150.id==160001);
  prefers3150.note(demand,BRIDGE);
  assert(prefers3150.id==CONTROLLER && prefers3150.bridgeDemandSeen);
  prefers3150.note(provisionalReply,BRIDGE);
  assert(prefers3150.id==CONTROLLER);
  // Codex P2: a previously bound virtual 01: can appear in the FIRST 3150
  // after reboot. When the real Evohome appears later, the two competing
  // 3150 targets are ambiguous: never enable pairing using a guessed ID.
  ramses_peer::ExistingController competing{};
  ramses_read::putAddress(demand.addresses[2],1,160001);
  competing.note(demand,BRIDGE);
  assert(competing.id==160001 && competing.bridgeDemandSeen);
  assert(!competing.ambiguous);
  ramses_read::putAddress(demand.addresses[2],1,CONTROLLER);
  competing.note(demand,BRIDGE);
  assert(competing.id==0 && competing.ambiguous);
  competing.note(demand,BRIDGE);
  assert(competing.id==0 && competing.ambiguous);
  // Neither a 3EF0 RP nor an unrelated bridge can clear that blocker.
  competing.note(provisionalReply,BRIDGE);
  competing.note(demand,BRIDGE+1);
  assert(competing.id==0 && competing.ambiguous);
  // Only a directly heard 01: -> configured bridge RQ resolves it.
  ramses::Message clearConflict{};
  clearConflict.messageType=0;clearConflict.addrMask=3;
  clearConflict.opcode=0x3EF0;
  ramses_read::putAddress(clearConflict.addresses[0],1,CONTROLLER);
  ramses_read::putAddress(clearConflict.addresses[1],10,BRIDGE);
  competing.note(clearConflict,BRIDGE);
  assert(competing.id==CONTROLLER && competing.directRequestSeen);
  assert(!competing.ambiguous);
  ramses_read::putAddress(demand.addresses[2],1,160001);
  competing.note(demand,BRIDGE);
  assert(competing.id==CONTROLLER && !competing.ambiguous);
  // The reverse order is also ambiguous, not merely first-virtual-first-real.
  ramses_peer::ExistingController reversed{};
  ramses_read::putAddress(demand.addresses[2],1,CONTROLLER);
  reversed.note(demand,BRIDGE);
  ramses_read::putAddress(demand.addresses[2],1,160001);
  reversed.note(demand,BRIDGE);
  assert(reversed.id==0 && reversed.ambiguous);
  // A repeated 3150 for one controller alone is not a conflict.
  ramses_peer::ExistingController repeated{};
  repeated.note(demand,BRIDGE);
  repeated.note(demand,BRIDGE);
  assert(repeated.id==160001 && !repeated.ambiguous);
  ramses_read::putAddress(demand.addresses[2],1,CONTROLLER);

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
  // Direct request retains precedence over subsequent bridge announcements.
  ramses_read::putAddress(demand.addresses[2],1,160001);
  observed.note(demand,BRIDGE);
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
