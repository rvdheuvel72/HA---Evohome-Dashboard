#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <initializer_list>
#include <string.h>
#include "../RamsesGateway/RamsesPairedBridgeRead.h"
using namespace ramses_bridge_read;
int main(){
  uint8_t f[16]={};size_t n=0;
  assert(makeRead(245914,48994,25,f,n) && n==16);
  ramses::Message m{};
  assert(ramses::decode(f,n,m));
  assert(m.messageType==0 && m.opcode==0x3220 && m.length==5);
  assert(Probe::ramses_peer_type(m.addresses[0])==1);
  assert(Probe::ramses_peer_id(m.addresses[0])==245914);
  assert(Probe::ramses_peer_type(m.addresses[1])==10);
  assert(Probe::ramses_peer_id(m.addresses[1])==48994);
  assert(m.payload[0]==0 && m.payload[2]==25);
  assert((m.payload[1]&0x70)==0); // OpenTherm Read-Data ONLY
  assert(!makeRead(245914,48994,0,f,n)); // status/control deliberately blocked
  assert(!makeRead(245914,48994,56,f,n)); // setpoint controls deliberately blocked
  assert(!makeRead(245914,48994,255,f,n));
  assert(!makeRead(0,48994,25,f,n));
  assert(!makeRead(245914,0,25,f,n));
  for(uint8_t id:{17,18,19,25,26,27,28,33})
    assert(makeRead(245914,48994,id,f,n));
  Probe p;
  assert(p.request(1000,245914,48994,25));
  assert(!p.request(1001,245914,48994,25)); // only one in-flight
  assert(p.begin(1100));
  assert(!p.begin(1101));
  assert(p.state==WAITING && p.busy());
  ramses::Message reply{};
  reply.messageType=3;reply.addrMask=3;reply.opcode=0x3220;reply.length=5;
  ramses_read::putAddress(reply.addresses[0],10,48994);
  ramses_read::putAddress(reply.addresses[1],1,245914);
  reply.payload[0]=0;reply.payload[1]=0x40;reply.payload[2]=25;
  reply.payload[3]=0x22;reply.payload[4]=0x80; // 34.5 C, f8.8
  uint32_t bits=(static_cast<uint32_t>(reply.payload[1])<<24)|
     (static_cast<uint32_t>(reply.payload[2])<<16)|
     (static_cast<uint32_t>(reply.payload[3])<<8)|reply.payload[4];
  if(ot_readonly::parity(bits))reply.payload[1]|=0x80;
  auto wrong=reply;
  ramses_read::putAddress(wrong.addresses[0],10,48995);
  assert(!p.observe(wrong,1200));
  wrong=reply;ramses_read::putAddress(wrong.addresses[1],1,245915);
  assert(!p.observe(wrong,1200));
  wrong=reply;wrong.payload[2]=28;
  assert(!p.observe(wrong,1200));
  assert(p.observe(reply,1200));
  assert(p.state==READ_ACK && p.raw==0x2280 && !p.busy());
  float temperature=0;
  assert(ot_readonly::fixedPoint(p.dataId,p.raw,temperature));
  assert(temperature==34.5f);
  assert(!p.request(1201,245914,48994,28));
  assert(p.request(61100,245914,48994,28));
  assert(p.begin(61110));
  assert(!p.observe(reply,61120)); // correct source but wrong OT id
  p.sent(false);
  assert(p.state==TX_FAILED && p.cooldown(61111)>0);
  assert(!p.request(61112,245914,48994,28));
  assert(p.request(121110,245914,48994,25));
  assert(p.begin(121111));
  assert(!p.observe(reply,122911)); // exactly timeout boundary
  assert(p.state==TIMEOUT);
  assert(!p.observe(reply,122912));
  Probe wrap;
  assert(wrap.request(0xfffffff0u,245914,48994,25));
  assert(wrap.begin(0xfffffff8u));
  assert(wrap.observe(reply,1000u)); // millis wrap
  Probe late;
  assert(late.request(200000,245914,48994,25));
  assert(late.begin(200010));
  ramses::Message invalid=reply;
  invalid.payload[1]^=0x80; // OT parity invalid
  assert(!late.observe(invalid,200050));
  assert(late.observe(reply,200060));
  puts("Real-controller-identity R8810A read-only probe tests PASS");
  return 0;
}
