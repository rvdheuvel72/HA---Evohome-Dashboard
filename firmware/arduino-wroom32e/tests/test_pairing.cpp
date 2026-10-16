#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../RamsesGateway/RamsesBinding.h"
int main() {
  uint8_t f[64]={}; size_t n=0;
  assert(ramses_bind::makeOffer(62142,48994,f,n) && n==29);
  ramses::Message offer{};
  assert(ramses::decode(f,n,offer));
  assert(offer.messageType==1 && offer.opcode==0x1FC9 && offer.length==18);
  assert(offer.addrMask==5 && ramses_bind::type(offer.addresses[0])==1);
  assert(ramses_bind::id(offer.addresses[0])==62142);
  assert(ramses_bind::id(offer.addresses[2])==62142);
  assert(offer.payload[0]==0 && offer.payload[1]==0 && offer.payload[2]==8);
  assert(offer.payload[6]==0xfc && offer.payload[7]==0x3b);
  assert(offer.payload[13]==0x1f && offer.payload[14]==0xc9);
  uint8_t wire[128]={}; size_t len=ramses_read::encodeUART(f,n,wire,sizeof(wire));
  assert(len==70);
  ramses::FrameDecoder decoder; ramses::Message decoded{};bool good=false;
  for(size_t i=0;i<len;i++)if(decoder.feed(wire[i],decoded))good=true;
  assert(good && decoded.opcode==0x1fc9 && decoded.messageType==1);
  assert(ramses_bind::makeConfirmation(62142,48994,f,n) && n==17);
  ramses::Message confirm{};
  assert(ramses::decode(f,n,confirm) && confirm.messageType==1);
  assert(confirm.addrMask==3 && confirm.length==6);
  assert(confirm.payload[0]==0 && confirm.payload[1]==0xff &&
         confirm.payload[2]==0xff);
  assert(ramses_bind::type(confirm.addresses[1])==10);
  uint8_t verify[16]={};
  assert(ramses_bind::makeVerifyRead(62142,48994,verify,n) && n==16);
  ramses::Message req{};
  assert(ramses::decode(verify,n,req) && req.opcode==0x3220);
  assert(ramses_bind::type(req.addresses[0])==1);
  assert(ramses_bind::id(req.addresses[0])==62142);
  assert(!ramses_bind::candidate(245914,48994,245914));
  assert(!ramses_bind::candidate(0,48994,245914));
  assert(!ramses_bind::candidate(262143,48994,245914));
  assert(!ramses_bind::makeOffer(0,48994,f,n));
  ramses_bind::Session s;
  assert(s.start(1000,62142,48994,245914));
  assert(!s.start(1001,62142,48994,245914));
  s.offered(true,1100);
  assert(s.state==ramses_bind::WAIT_RESPONSE);
  ramses::Message w{};
  w.messageType=2;w.opcode=0x1fc9;w.addrMask=3;w.length=6;
  ramses_read::putAddress(w.addresses[0],10,48994);
  ramses_read::putAddress(w.addresses[1],1,62142);
  w.payload[0]=0;w.payload[1]=0x3e;w.payload[2]=0xf0;
  ramses_read::putAddress(w.payload+3,10,48994);
  ramses::Message fake=w;
  ramses_read::putAddress(fake.addresses[0],10,48995);
  assert(!s.observe(fake,1200));
  fake=w;fake.payload[1]=0;
  assert(!s.observe(fake,1200));
  fake=w;ramses_read::putAddress(fake.addresses[1],1,62143);
  assert(!s.observe(fake,1200));
  assert(s.observe(w,1250) && s.acceptedOffer);
  assert(s.state==ramses_bind::CONFIRM_QUEUED);
  s.confirmed(true);
  assert(s.confirmationSent && s.state==ramses_bind::VERIFY_QUEUED);
  s.verifiedRequestSent(true,1400);
  assert(s.state==ramses_bind::WAIT_VERIFY);
  ramses::Message rp{};
  rp.messageType=3;rp.opcode=0x3220;rp.addrMask=3;rp.length=5;
  ramses_read::putAddress(rp.addresses[0],10,48994);
  ramses_read::putAddress(rp.addresses[1],1,62142);
  const uint8_t otReply[5]={0x00,0xC0,0x19,0x25,0x80};
  memcpy(rp.payload,otReply,5);
  // Add OpenTherm parity as necessary.
  uint32_t bits=(static_cast<uint32_t>(rp.payload[1])<<24)|
     (static_cast<uint32_t>(rp.payload[2])<<16)|
     (static_cast<uint32_t>(rp.payload[3])<<8)|rp.payload[4];
  if(ot_readonly::parity(bits))rp.payload[1]|=0x80;
  assert(s.observe(rp,1500) && s.state==ramses_bind::VERIFIED);
  assert(s.verifiedReadAck && s.lastRaw==0x2580);
  assert(s.cooldown(1600)>0 && !s.start(1600,63111,48994,245914));
  assert(s.start(61000,63111,48994,245914));
  s.offered(true,61001);
  s.tick(76001);
  assert(s.state==ramses_bind::TIMED_OUT && !s.confirmationSent);
  assert(s.start(121000,63111,48994,245914));
  s.offered(true,121001);
  ramses_read::putAddress(w.addresses[1],1,63111);
  assert(s.observe(w,121002));
  s.confirmed(true);
  s.verifiedRequestSent(true,121003);
  s.tick(123503);
  assert(s.state==ramses_bind::UNVERIFIED);
  assert(s.start(181000,63111,48994,245914));
  s.offered(true,181001);
  ramses::Message conflict{};
  conflict.addrMask=1;conflict.messageType=0;
  ramses_read::putAddress(conflict.addresses[0],1,63111);
  assert(!s.observe(conflict,181002));
  assert(s.state==ramses_bind::ID_CONFLICT);
  printf("Manual R8810A binding encoders, RX gates, timers and verification PASS\n");
  return 0;
}
