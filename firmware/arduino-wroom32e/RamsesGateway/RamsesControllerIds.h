// Bounded RF controller identity observations for deliberate 1FC9 pairing.
// Exceeding capacity MUST disable further pairing rather than silently losing
// collision protection. Arduino-independent for host-native regression tests.
#pragma once
#include <stdint.h>
namespace ramses_ids {
struct SeenControllers {
  static constexpr uint8_t CAPACITY=12;
  uint32_t ids[CAPACITY]={};
  uint8_t count=0;
  bool contains(uint32_t id) const {
    for(uint8_t i=0;i<count;i++)if(ids[i]==id)return true;
    return false;
  }
  bool saturated() const {return count>=CAPACITY;}
  void note(uint32_t id) {
    if(!id || contains(id) || saturated())return;
    ids[count++]=id;
  }
  // Deliberately fail closed at capacity: we cannot rule out collisions with
  // further live controller identities once our bounded list is full.
  bool eligible(uint32_t candidate) const {
    return candidate>0 && candidate<262143 && !saturated() &&
           !contains(candidate);
  }
};
} // namespace ramses_ids
