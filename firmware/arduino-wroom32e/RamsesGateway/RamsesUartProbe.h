// Bounded, passive UART1 RX byte evidence for diagnosing RAMSES RF input.
// Not an RF transmitter, packet injection mechanism, or hardware sampler.
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ramses_uart_probe {
constexpr size_t CAPACITY=48;
struct Snapshot {
  uint32_t received=0;
  uint32_t zeros=0;
  uint32_t all_ones=0;
  uint8_t last[CAPACITY]={};
  uint8_t count=0;
  uint8_t next=0;
  void observe(uint8_t value) {
    ++received;
    if(value==0)++zeros;
    if(value==0xFFu)++all_ones;
    last[next]=value;
    next=static_cast<uint8_t>((next+1u)%CAPACITY);
    if(count<CAPACITY)++count;
  }
  // Oldest to newest, suitable for comparing real UART input with sync.
  size_t toHex(char* out,size_t capacity) const {
    if(!out || capacity < 2u*count+1u)return 0;
    static constexpr char digits[]="0123456789ABCDEF";
    const size_t first=(next+CAPACITY-count)%CAPACITY;
    for(size_t i=0;i<count;++i) {
      const uint8_t v=last[(first+i)%CAPACITY];
      out[i*2]=digits[v>>4];
      out[i*2+1]=digits[v&0x0Fu];
    }
    out[count*2]=0;
    return static_cast<size_t>(count)*2;
  }
};
} // namespace ramses_uart_probe
