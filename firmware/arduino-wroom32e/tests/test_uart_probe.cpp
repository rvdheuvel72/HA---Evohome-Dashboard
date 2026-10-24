#include <cassert>
#include <cstring>
#include <cstdio>
#include "../RamsesGateway/RamsesUartProbe.h"

int main() {
  using namespace ramses_uart_probe;
  Snapshot s;
  char output[2*CAPACITY+1]={};
  assert(s.toHex(output,sizeof(output))==0);
  assert(strcmp(output,"")==0);
  s.observe(0x00);
  s.observe(0xFF);
  s.observe(0x35);
  assert(s.received==3 && s.zeros==1 && s.all_ones==1);
  assert(s.toHex(output,sizeof(output))==6);
  assert(strcmp(output,"00FF35")==0);
  for(unsigned i=0;i<CAPACITY+5u;++i) s.observe(static_cast<uint8_t>(i));
  assert(s.count==CAPACITY && s.received==CAPACITY+8u);
  assert(s.toHex(output,sizeof(output))==2*CAPACITY);
  assert(strncmp(output,"05",2)==0);
  assert(strcmp(output+2*(CAPACITY-1u),"34")==0);
  assert(s.toHex(output,2*CAPACITY)==0); // prevent undersized buffer
  puts("Passive UART probe ring tests PASS");
}
