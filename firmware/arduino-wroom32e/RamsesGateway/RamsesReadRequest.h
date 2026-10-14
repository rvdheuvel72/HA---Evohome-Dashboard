// Constrained RAMSES-II RQ/3220 packet encoder. No "W", zone overrides,
// binding, actuator commands, or arbitrary user-provided RF frames.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "OpenThermReadOnly.h"
namespace ramses_read {
static const uint8_t MANCHESTER[16]={
  0xAA,0xA9,0xA6,0xA5,0x9A,0x99,0x96,0x95,
  0x6A,0x69,0x66,0x65,0x5A,0x59,0x56,0x55
};
inline void putAddress(uint8_t* out,uint8_t type,uint32_t id){
  out[0]=static_cast<uint8_t>((type<<2)|((id>>16)&3));
  out[1]=static_cast<uint8_t>(id>>8);
  out[2]=static_cast<uint8_t>(id);
}
inline bool makeRequest(uint32_t source18,uint32_t bridge10,uint8_t id,
                        uint8_t out[16],size_t& size){
  size=0;
  if (!out || !source18 || source18>=262143 || source18==730 ||
      !bridge10 || bridge10>262143) return false;
  // Header 0x0C = verb RQ, addresses slots 0+1, no sequence parameters.
  out[0]=0x0c;
  putAddress(out+1,18,source18);
  putAddress(out+4,10,bridge10);
  out[7]=0x32;out[8]=0x20;out[9]=5;
  if (!ot_readonly::request(id,out+10))return false;
  uint8_t sum=0;
  for(size_t i=0;i<15;i++)sum=static_cast<uint8_t>(sum+out[i]);
  out[15]=static_cast<uint8_t>(0u-sum);size=16;
  return true;
}
// For 16 bytes RAMSES frame: 5 preamble + 2 sync + 3 header +
// 32 Manchester-coded bytes + 2 trailer = 44 UART bytes.
inline size_t encodeUART(const uint8_t* msg,size_t size,uint8_t* out,size_t cap){
  if (!msg || !out || size>64 || cap<12+2*size)return 0;
  size_t n=0;
  for(int i=0;i<5;i++)out[n++]=0x55;
  out[n++]=0xff;out[n++]=0x00;
  out[n++]=0x33;out[n++]=0x55;out[n++]=0x53;
  for(size_t i=0;i<size;i++){
    out[n++]=MANCHESTER[msg[i]>>4];
    out[n++]=MANCHESTER[msg[i]&15u];
  }
  out[n++]=0x35;out[n++]=0x55;
  return n;
}
} // namespace ramses_read
