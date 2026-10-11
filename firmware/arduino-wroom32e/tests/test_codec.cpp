#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "../RamsesGateway/RamsesCodec.h"

static const uint8_t manchester[16] = {
  0xAA, 0xA9, 0xA6, 0xA5, 0x9A, 0x99, 0x96, 0x95,
  0x6A, 0x69, 0x66, 0x65, 0x5A, 0x59, 0x56, 0x55
};

static std::vector<uint8_t> makeFrame(std::vector<uint8_t> raw) {
  uint8_t sum = 0;
  for (const uint8_t b : raw) sum = static_cast<uint8_t>(sum + b);
  raw.push_back(static_cast<uint8_t>(0 - sum));
  std::vector<uint8_t> frame = {0x55, 0x55, 0xFF, 0x00, 0x33, 0x55, 0x53};
  for (const uint8_t b : raw) {
    frame.push_back(manchester[b >> 4]);
    frame.push_back(manchester[b & 15]);
  }
  frame.push_back(0x35);
  return frame;
}
int main() {
  // Type I, source and third address, no params, opcode 0x30C9, payload 0x64.
  const std::vector<uint8_t> raw = {
    0x18, 0x05, 0xE2, 0x40, 0x10, 0x00, 0x7B, 0x30, 0xC9, 0x01, 0x64
  };
  ramses::FrameDecoder decoder;
  ramses::Message result{};
  int frames = 0;
  const auto frame = makeFrame(raw);
  for (const auto b : frame) if (decoder.feed(b, result)) ++frames;
  assert(frames == 1);
  assert(result.messageType == 1);
  assert(result.addrMask == 0x05);
  assert(result.opcode == 0x30C9);
  assert(result.length == 1);
  assert(result.payload[0] == 0x64);
  char line[256]{};
  assert(ramses::format(result, 70, line, sizeof(line)));
  assert(strstr(line, "01:123456") != nullptr);
  assert(strstr(line, "04:000123") != nullptr);
  assert(strstr(line, "30C9 001 64") != nullptr);
  decoder.reset();

  // Corrupt one Manchester-valid symbol => checksum rejection.
  auto corrupt = frame;
  corrupt[9] = corrupt[9] == 0xAA ? 0xA9 : 0xAA;
  frames = 0;
  for (const auto b : corrupt) if (decoder.feed(b, result)) ++frames;
  assert(frames == 0);

  // Invalid Manchester symbol => rejection.
  auto invalid = frame;
  invalid[12] = 0xF0;
  decoder.reset();
  frames = 0;
  for (const auto b : invalid) if (decoder.feed(b, result)) ++frames;
  assert(frames == 0);

  // Truncation => failure even when checksum is internally correct.
  auto truncated = raw;
  truncated[9] = 0x02; // change length: claims 2 payload bytes, only 1 present
  decoder.reset();
  frames = 0;
  for (const auto b : makeFrame(truncated)) if (decoder.feed(b, result)) ++frames;
  assert(frames == 0);

  puts("RAMSES Manchester, checksum, address, format, invalid and truncation tests PASS");
  return 0;
}
