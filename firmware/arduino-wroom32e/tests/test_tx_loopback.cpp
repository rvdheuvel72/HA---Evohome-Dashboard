#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../RamsesGateway/RamsesTxLoopback.h"
#include "../RamsesGateway/RamsesCodec.h"

int main() {
  using namespace ramses_loopback;
  uint8_t fixture[MAX_WIRE]={};
  size_t n=0;
  assert(makeFixture(fixture,sizeof(fixture),n) && n==70);
  assert(!makeFixture(fixture,69,n) && n==0);
  assert(!makeFixture(nullptr,sizeof(fixture),n));
  assert(TX_GPIO==26 && RX_GPIO==25);
  Report d;
  assert(strcmp(name(d.state),"not_run")==0);
  assert(compare(d)==INVALID_FIXTURE);
  assert(makeFixture(d.expected,sizeof(d.expected),n));
  d.expected_len=static_cast<uint16_t>(n);
  assert(compare(d)==NO_SIGNAL);
  memcpy(d.captured,d.expected,n);
  d.captured_len=static_cast<uint16_t>(n);
  assert(compare(d)==PASS && d.first_difference==-1);
  ramses::FrameDecoder decoder;
  ramses::Message msg;
  bool decoded=false;
  for(size_t i=0;i<n;i++)if(decoder.feed(d.captured[i],msg))decoded=true;
  assert(decoded && msg.opcode==0x1FC9 && msg.messageType==1);
  assert(msg.length==18 && ramses_bind::id(msg.addresses[0])==160001);
  d.captured[12]^=0x01;
  assert(compare(d)==DIFFERENT_BYTES && d.first_difference==12);
  d.captured[12]^=0x01;
  d.captured_len=69;
  assert(compare(d)==SHORT_CAPTURE && d.first_difference==-1);
  d.captured_len=71;
  d.captured[70]=0xff;
  assert(compare(d)==EXTRA_BYTES && d.first_difference==-1);
  d.captured_len=97;
  assert(compare(d)==INVALID_FIXTURE);
  d.captured_len=70;
  char hex[MAX_WIRE*2+1]={};
  assert(hexBytes(d.expected,70,hex,sizeof(hex)));
  assert(strlen(hex)==140);
  assert(strncmp(hex,"5555555555FF00335553",20)==0);
  assert(!hexBytes(d.expected,70,hex,140));
  assert(!hexBytes(d.expected,97,hex,sizeof(hex)));
  assert(strcmp(name(UNSAFE_RF_STATE),"unsafe_radio_state")==0);
  puts("GPIO26-to-GPIO25 UART 1FC9 loopback fixture and comparisons PASS");
}
