// Convert the 44 legacy RAMSES UART bytes into a CC1101 FIFO bitstream.
// Each UART byte is 8N1: START=0, 8 payload bits LSB first, STOP=1.
// FIFO transmits MSB first; 44 * 10 bits = 440 bits = exactly 55 bytes.
// Note: frame.c tx_byte() uses MARK, START, DATA (1,0,LSB-first)
// at the beginning of each octet. Our legacy encode() uses START, DATA,
// STOP (0,LSB-first,1), which is a one-bit stream phase shift.
// Reference FIFO must use encodeReferenceSerial() instead.
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ramses_uart_fifo {
constexpr size_t INPUT_LEN=44;
constexpr size_t RF_LEN=55;
 // IndaloTech/ramses_esp tx_fifo_prime(): one 0x00 priming byte, then
 // 0xFF 0x00 0x00 break; tx_flush() finishes with 0xFF idle MARK.
 // We retain *one* bounded, fixed-length packet (60 bytes <= CC1101 64-byte FIFO).
constexpr size_t PRIME_LEN=4;
constexpr size_t IDLE_LEN=1;
constexpr size_t REFERENCE_RF_LEN=PRIME_LEN+RF_LEN+IDLE_LEN;
static_assert(REFERENCE_RF_LEN==60,"One-shot TX must fit CC1101 FIFO");

inline bool encode(const uint8_t* serial,size_t serialLen,
                   uint8_t* out,size_t capacity,size_t& used) {
  used=0;
  if(!serial || !out || serialLen!=INPUT_LEN || capacity<RF_LEN)return false;
  for(size_t i=0;i<RF_LEN;i++)out[i]=0;
  size_t bitPos=0;
  const auto writeBit=[&](uint8_t b) {
    if(b)out[bitPos>>3] |= static_cast<uint8_t>(0x80u>>(bitPos&7));
    ++bitPos;
  };
  for(size_t i=0;i<serialLen;i++){
    writeBit(0); // UART start bit: SPACE
    const uint8_t value=serial[i];
    for(unsigned b=0;b<8;b++)writeBit(static_cast<uint8_t>((value>>b)&1u));
    writeBit(1); // UART stop bit: MARK
  }
  used=bitPos/8;
  return bitPos==RF_LEN*8 && used==RF_LEN;
}
// Exact on-air data bit alignment of IndaloTech/ramses_esp frame.c
// tx_byte(): emit MARK=1, START=0, then DATA LSB-first for every
// UART byte. Relative to START+DATA+STOP, this rotates the serial stream
// one bit: essential after the reference's 00 FF 00 00 BREAK primer.
// This encoder is independent of the legacy serial-UART framing method.
inline bool encodeReferenceSerial(const uint8_t* serial,size_t serialLen,
                                  uint8_t* out,size_t capacity,size_t& used) {
  used=0;
  if(!serial || !out || serialLen!=INPUT_LEN || capacity<RF_LEN)return false;
  for(size_t i=0;i<RF_LEN;i++)out[i]=0;
  size_t bitPos=0;
  const auto putBit=[&](uint8_t bit){
    if(bit)out[bitPos>>3]|=static_cast<uint8_t>(0x80u>>(bitPos&7));
    ++bitPos;
  };
  for(size_t i=0;i<serialLen;i++){
    putBit(1); // MARK before each UART start, as in reference insert_ps()
    putBit(0); // START=SPACE
    const uint8_t c=serial[i];
    for(unsigned b=0;b<8;b++)putBit(static_cast<uint8_t>((c>>b)&1u));
  }
  used=bitPos/8;
  return bitPos==RF_LEN*8 && used==RF_LEN;
}

// Bounded representation of reference FIFO priming + exactly the upstream
// MARK/START/DATA waveform + trailing MARK. The primer adds no RQ message.
inline bool encodePrimedReference(const uint8_t* serial,size_t serialLen,
                                  uint8_t* out,size_t capacity,size_t& used) {
  used=0;
  if(!serial || !out || serialLen!=INPUT_LEN ||
     capacity<REFERENCE_RF_LEN)return false;
  const uint8_t prefix[PRIME_LEN]={0x00,0xFF,0x00,0x00};
  for(size_t i=0;i<PRIME_LEN;i++)out[i]=prefix[i];
  size_t frameLength=0;
  if(!encodeReferenceSerial(serial,serialLen,out+PRIME_LEN,
                             capacity-PRIME_LEN,frameLength) ||
     frameLength!=RF_LEN)return false;
  out[PRIME_LEN+frameLength]=0xFF;
  used=PRIME_LEN+frameLength+IDLE_LEN;
  return used==REFERENCE_RF_LEN;
}
} // namespace ramses_uart_fifo
