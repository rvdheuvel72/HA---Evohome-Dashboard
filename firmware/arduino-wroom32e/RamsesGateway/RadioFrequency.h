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
inline bool withinChipBand(uint32_t khz){
  return (khz>=300000 && khz<=348000) ||
         (khz>=387000 && khz<=464000) ||
         (khz>=779000 && khz<=928000);
}
// Cross-band tuning needs an explicit RX-only configuration opt-in.
inline bool synth(uint32_t khz,uint16_t band,uint32_t& word,bool experimentalRx=false){
  if(!withinHardwareBand(khz,band) &&
     !(experimentalRx && withinChipBand(khz)))return false;
  // CC1101 nominal XOSC = 26 MHz; FREQ = f_RFx2^16/f_XOSC.
  const uint64_t rounded=static_cast<uint64_t>(khz)*65536u+13000u;
  const uint64_t v=rounded/26000u;
  if(v>0xFFFFFFu)return false;
  word=static_cast<uint32_t>(v);
  return true;
}
// Only for a single, manually authorized RQ 3220 Read-Data ID25.
inline bool experimentalOneShotFrequency(uint32_t khz,uint16_t band,bool crossRx){
  return khz==868300 && band==433 && crossRx;
}
inline bool evohomeTxFrequency(uint32_t khz,uint16_t band){
  // Avoid transmitting a RAMSES request on an arbitrary tuning.
  return band==868 && khz==868300;
}
 // Only bound 01: controllers may use the standard-band one-shot exception.
 // Normal unpaired 18: one-shot path remains the existing guarded experiment.
inline bool manualReadOneShotAllowed(uint32_t khz,uint16_t band,
                                     bool crossBandRx,bool verifiedBinding) {
  return experimentalOneShotFrequency(khz,band,crossBandRx) ||
      (verifiedBinding && evohomeTxFrequency(khz,band));
}
// Binding verification authorizes the distinct 01: identity; it must not
// depend on a configured 18: ID or an 18: address conflict. An unpaired
// source still needs a valid unique 18: identity and the restricted one-shot.
inline bool manualReadOneShotWithIdentity(uint32_t khz,uint16_t band,
                                         bool crossBandRx,bool verifiedBinding,
                                         uint32_t unpairedSource18,
                                         bool unpairedSourceConflict) {
  if(verifiedBinding)
    return manualReadOneShotAllowed(khz,band,crossBandRx,true);
  return unpairedSource18>0 && unpairedSource18<262143 &&
         unpairedSource18!=730 && !unpairedSourceConflict &&
         experimentalOneShotFrequency(khz,band,crossBandRx);
}
} // namespace radio_frequency
