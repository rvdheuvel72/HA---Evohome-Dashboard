#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <initializer_list>
#include <string.h>
#include "../RamsesGateway/RamsesPairedBridgeRead.h"
#include "../RamsesGateway/CvReadBatch.h"
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
  assert(!makeRead(245914,48994,0,f,n)); // status master flags never transmitted
  assert(!makeRead(245914,48994,255,f,n));
  assert(!makeRead(0,48994,25,f,n));
  assert(!makeRead(245914,0,25,f,n));
  for(uint8_t id:{5,17,18,19,25,26,27,28,33,57})
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
  // CV button uses the same single-read hardware path at most once per ID.
  cv_read_batch::Batch batch;
  assert(cv_read_batch::COUNT==8);
  assert(batch.start(1000));
  assert(batch.active && batch.due(1000) && batch.id()==17);
  assert(!batch.start(1001)); // no double-start while active
  batch.queued();
  assert(!batch.due(1001));
  ramses_bridge_read::Probe response;
  response.state=ramses_bridge_read::WAITING;
  response.dataId=17;
  ot_tx::Report localTx;
  localTx.phase=ot_tx::LOCAL_COMPLETE;
  localTx.expected_bytes=44;
  localTx.written_bytes=44;
  assert(!batch.settle(response,localTx,1200)); // no unconfirmed RX
  response.state=ramses_bridge_read::READ_ACK;
  response.raw=0x0000;
  assert(batch.settle(response,localTx,1300));
  assert(batch.next==1 && batch.items[0].id==17);
  assert(batch.items[0].state==ramses_bridge_read::READ_ACK);
  assert(batch.items[0].txBytes==44 && batch.items[0].txExpectedBytes==44);
  assert(!batch.due(61299));
  assert(batch.due(61300) && batch.id()==18);
  batch.queued();
  response.dataId=18;response.state=ramses_bridge_read::TIMEOUT;
  assert(batch.settle(response,localTx,63000));
  assert(batch.items[1].state==ramses_bridge_read::TIMEOUT);
  assert(batch.next==2 && batch.id()==25 && !batch.due(123000-1));
  assert(batch.due(123000));
  // If a future RF preflight remains blocked, fail closed; never retry the ID.
  assert(!batch.overdue(243000));
  assert(batch.overdue(243001));
  batch.abort(243001);
  assert(!batch.active && batch.aborted && batch.completed);
  assert(!batch.due(243002));
  assert(batch.start(244000)); // new click explicitly resets the result set
  assert(batch.items[0].state==ramses_bridge_read::IDLE);
  assert(batch.items[0].id==17 && batch.items[7].id==26);
  assert(!batch.start(244001));
  // All IDs are deliberate, read-only, and outside ID 0/27 and write controls.
  for(uint8_t id:cv_read_batch::IDS) {
    assert(id!=0 && id!=27 && id!=56);
    assert(makeRead(245914,48994,id,f,n));
  }
  puts("Real-controller-identity R8810A read-only probe tests PASS");
  return 0;
}
