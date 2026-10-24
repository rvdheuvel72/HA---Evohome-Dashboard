#include <initializer_list>
#include <cassert>
#include <cstdio>
#include "../RamsesGateway/Cc1101Startup.h"
int main() {
  using namespace cc1101_startup;
  assert(identified(0x00,0x14));
  assert(!identified(0x00,0xFF));
  assert(!identified(0x00,0x00));
  assert(!identified(0xFF,0x14));
  // An RX strobe accepted by a genuine chip is not a failed SPI discovery
  // simply because the immediate MARCSTATE is settling or IDLE.
  for(uint8_t state: {static_cast<uint8_t>(0x01),static_cast<uint8_t>(0x0D),
                      static_cast<uint8_t>(0x0E),static_cast<uint8_t>(0x1F)}) {
    assert(ready(0x00,0x14,true));
    assert(receiving(state)==(state==0x0D));
  }
  assert(!ready(0x00,0x14,false));
  assert(!ready(0xFF,0xFF,true));
  assert(decodeMarcState(0xFF)==0xFF);
  assert(decodeMarcState(0x8D)==0x0D);
  assert(decodeMarcState(0x01)==0x01);
  assert(!receiving(decodeMarcState(0xFF)));
  assert(receiving(0x8D)); // status register may contain unrelated upper bits
  std::puts("CC1101 boot identification independent from transient RX state PASS");
}
