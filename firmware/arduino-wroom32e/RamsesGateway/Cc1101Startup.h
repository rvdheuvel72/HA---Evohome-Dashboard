// Pure CC1101 boot acceptance policy, testable without ESP32 hardware.
// Chip identification + completed register programming + accepted SRX strobe
// establish radio initialization; MARCSTATE is an advisory RX health reading,
// never a second chip-detection gate.
#pragma once
#include <stdint.h>
namespace cc1101_startup {
constexpr bool identified(uint8_t part,uint8_t version) {
  return part==0x00 && version!=0x00 && version!=0xFF;
}
constexpr bool ready(uint8_t part,uint8_t version,bool srxAccepted) {
  return identified(part,version) && srxAccepted;
}
// A failed SPI status read yields 0xFF. Masking it to five bits made
// the diagnostic falsely show MARCSTATE=31 (which is not RX).
constexpr uint8_t decodeMarcState(uint8_t raw) {
  return raw==0xFFu?0xFFu:static_cast<uint8_t>(raw&0x1Fu);
}
constexpr bool receiving(uint8_t marcstate) {
  return (marcstate&0x1Fu)==0x0Du;
}
} // namespace cc1101_startup
