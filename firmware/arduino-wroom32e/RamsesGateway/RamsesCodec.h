// RAMSES II receive decoder (Arduino-independent).
// Algorithm and RF symbols adapted from IndaloTech/ramses_esp by Peter Price (2023).
// No transmit implementation. Review upstream license before redistribution.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace ramses {

enum RejectReason : uint8_t {
  NO_REJECTION=0, INVALID_SYMBOL, BAD_CHECKSUM, MALFORMED_FRAME,
  OVERSIZED_FRAME, INCOMPLETE_FRAME, IDLE_TIMEOUT
};
inline const char* rejectName(RejectReason reason) {
  switch(reason) {
    case NO_REJECTION:return "none";
    case INVALID_SYMBOL:return "invalid_manchester";
    case BAD_CHECKSUM:return "bad_checksum";
    case MALFORMED_FRAME:return "malformed_structure";
    case OVERSIZED_FRAME:return "oversized_frame";
    case INCOMPLETE_FRAME:return "incomplete_frame";
    case IDLE_TIMEOUT:return "idle_timeout";
  }
  return "unknown";
}

struct Message {
  uint8_t header = 0;
  uint8_t messageType = 0;
  uint8_t addrMask = 0;
  uint8_t addresses[3][3] = {};
  uint8_t params[2] = {};
  bool hasParam[2] = {};
  uint16_t opcode = 0;
  uint8_t length = 0;
  uint8_t payload[64] = {};
};

inline bool decode(const uint8_t* raw, size_t size, Message& msg,
                   RejectReason* reason=nullptr) {
  if(reason)*reason=NO_REJECTION;
  const auto invalid=[reason](RejectReason error)->bool {
    if(reason)*reason=error;
    return false;
  };
  // Message: header, variable addresses/params, opcode, payload-length,
  // payload, checksum. Full-byte checksum adds to zero modulo 256.
  if (size < 5 || !raw || (raw[0] & 0xC0)) return invalid(MALFORMED_FRAME);
  const uint8_t addrMasks[4] = { 0x07, 0x04, 0x05, 0x03 };
  const uint8_t h = raw[0];
  Message candidate{};
  candidate.header = h;
  candidate.messageType = (h >> 4) & 0x03;
  candidate.addrMask = addrMasks[(h >> 2) & 0x03];
  size_t pos = 1;

  for (uint8_t i = 0; i < 3; ++i) {
    if (candidate.addrMask & (1u << i)) {
      if (size - pos < 3) return invalid(MALFORMED_FRAME);
      memcpy(candidate.addresses[i], raw + pos, 3);
      pos += 3;
    }
  }

  for (uint8_t i = 0; i < 2; ++i) {
    if (h & (i == 0 ? 0x02 : 0x01)) {
      if (pos >= size) return invalid(MALFORMED_FRAME);
      candidate.hasParam[i] = true;
      candidate.params[i] = raw[pos++];
    }
  }
  if (size - pos < 4) return invalid(MALFORMED_FRAME); // opcode(2), length(1), csum(1)
  candidate.opcode = (static_cast<uint16_t>(raw[pos]) << 8) | raw[pos + 1];
  pos += 2;
  const uint8_t payloadLen = raw[pos++];
  if (payloadLen > sizeof(candidate.payload) || size - pos != static_cast<size_t>(payloadLen) + 1u) return invalid(MALFORMED_FRAME);
  candidate.length = payloadLen;
  memcpy(candidate.payload, raw + pos, payloadLen);
  uint8_t sum = 0;
  for (size_t i = 0; i < size; ++i) sum = static_cast<uint8_t>(sum + raw[i]);
  if (sum != 0) return invalid(BAD_CHECKSUM);
  msg = candidate;
  return true;
}

inline bool format(const Message& msg, uint8_t rssi, char* dest, size_t cap) {
  if (!dest || cap < 80) return false;
  const char* types[] = {"RQ", " I", " W", "RP"};
  char address[3][12] = {};
  for (uint8_t i = 0; i < 3; ++i) {
    if (msg.addrMask & (1u << i)) {
      const uint8_t* a = msg.addresses[i];
      const unsigned klass = a[0] >> 2;
      const unsigned long id = ((static_cast<unsigned long>(a[0] & 3)) << 16)
                             | (static_cast<unsigned long>(a[1]) << 8) | a[2];
      snprintf(address[i], sizeof(address[i]), "%02u:%06lu", klass, id);
    } else {
      snprintf(address[i], sizeof(address[i]), "--:------");
    }
  }
  char p[4];
  if (msg.hasParam[0]) snprintf(p, sizeof(p), "%03u", msg.params[0]);
  else memcpy(p, "---", 4);
  int used = snprintf(dest, cap, "%03u %2s %s %s %s %s %04X %03u ",
                      rssi, types[msg.messageType], p,
                      address[0], address[1], address[2],
                      msg.opcode, msg.length);
  if (used < 0 || static_cast<size_t>(used) >= cap) return false;
  for (uint8_t i = 0; i < msg.length; ++i) {
    const int n = snprintf(dest + used, cap - used, "%02X", msg.payload[i]);
    if (n != 2 || static_cast<size_t>(used + n) >= cap) return false;
    used += n;
  }
  return true;
}

class FrameDecoder {
 public:
  // The RF modem sends asynchronous data into a UART at 38400 baud.
  // Frame sync: FF 00 33 55 53; last four bytes as seen by UART: 00 33 55 53.
  bool feed(uint8_t value, Message& out) {
    if (!active_) {
      sync_ = (sync_ << 8) | value;
      if (sync_ == 0x00335553u) {
        ++syncCount_;
        active_ = true;
        size_ = 0;
        half_ = false;
      }
      return false;
    }
    if (value == 0x35) { // RAMSES trailer
      RejectReason reason=NO_REJECTION;
      const bool valid = !half_ && decode(raw_, size_, out,&reason);
      if(!valid)lastError_=half_?INCOMPLETE_FRAME:reason;
      reset();
      return valid;
    }
    const uint8_t lo = nibble(value & 0x0F);
    const uint8_t hi = nibble(value >> 4);
    if (lo == 0xFF || hi == 0xFF) {
      lastError_=INVALID_SYMBOL;
      reset();
      return false;
    }
    // Low physical nibble corresponds to high bits of a Manchester-decoded nibble.
    const uint8_t decodedNibble = static_cast<uint8_t>(lo | (hi << 2));
    if (!half_) {
      pending_ = decodedNibble << 4;
      half_ = true;
    } else {
      if (size_ >= sizeof(raw_)) { lastError_=OVERSIZED_FRAME;reset();return false; }
      raw_[size_++] = static_cast<uint8_t>(pending_ | decodedNibble);
      half_ = false;
    }
    return false;
  }

  void reset() { active_ = false; half_ = false; size_ = 0; sync_ = 0; }
  void abortIdleTimeout() { if(active_)lastError_=IDLE_TIMEOUT;reset(); }
  RejectReason takeError() {
    const RejectReason result=lastError_;
    lastError_=NO_REJECTION;
    return result;
  }
  bool receiving() const { return active_; }
  uint32_t syncCount() const { return syncCount_; }

 private:
  static uint8_t nibble(uint8_t bits) {
    // Matches the RAMSES Manchester symbol table.
    switch (bits) {
      case 0x05: return 3;
      case 0x06: return 2;
      case 0x09: return 1;
      case 0x0A: return 0;
      default: return 0xFF;
    }
  }
  RejectReason lastError_=NO_REJECTION;
  bool active_ = false;
  bool half_ = false;
  uint8_t pending_ = 0;
  uint32_t sync_ = 0;
  uint32_t syncCount_ = 0;
  size_t size_ = 0;
  uint8_t raw_[80] = {};
};

} // namespace ramses
