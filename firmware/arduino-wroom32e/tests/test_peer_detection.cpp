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
  printf("RAMSES bridge-to-controller passive 3EF0 inference PASS\n");
  return 0;
}
