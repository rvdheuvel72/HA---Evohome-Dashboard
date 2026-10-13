// Frequency synthesizer configuration for a declared CC1101 RF module band.
// Chip-level tunability does NOT certify a PCB or antenna at another band.
#pragma once
#include <stdint.h>
namespace radio_frequency {
inline bool withinHardwareBand(uint32_t khz,uint16_t declaredBand){
  switch(declaredBand){
    case 315:return khz>=300000 && khz<=348000;
    case 433:return khz>=387000 && khz<=464000;
    case 868:return khz>=863000 && khz<=870000;
    case 915:return khz>=902000 && khz<=928000;
    default:return false;
  }
}
inline bool synth(uint32_t khz,uint16_t band,uint32_t& word){
  if(!withinHardwareBand(khz,band))return false;
  // CC1101 nominal XOSC = 26 MHz; FREQ = f_RFx2^16/f_XOSC.
  const uint64_t rounded=static_cast<uint64_t>(khz)*65536u+13000u;
  const uint64_t v=rounded/26000u;
  if(v>0xFFFFFFu)return false;
  word=static_cast<uint32_t>(v);
  return true;
}
inline bool evohomeTxFrequency(uint32_t khz,uint16_t band){
  // Avoid transmitting a RAMSES request on an arbitrary tuning.
  return band==868 && khz==868300;
}
} // namespace radio_frequency
