#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "../RamsesGateway/RamsesCodec.h"
#include "../RamsesGateway/RamsesRxDiagnostics.h"

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

  // Correct signed CC1101 RSSI conversion including the 0x80 boundary.
  assert(ramses_rx::rssiDbm(0x00)==-74);
  assert(ramses_rx::rssiDbm(0x80)==-138);
  assert(ramses_rx::rssiDbm(0xFF)==-74);
  assert(ramses_rx::rssiMagnitude(0x80)==138);
  assert(ramses_rx::rssiMagnitude(0x00)==74);

  ramses_rx::Counters diag;
  // Reject classifications are observable even if sender cannot be recovered.
  ramses::FrameDecoder errors;
  ramses::Message unexpected;
  auto badChecksum=frame;
  badChecksum[9]=badChecksum[9]==0xAA?0xA9:0xAA;
  for(uint8_t byte:badChecksum)errors.feed(byte,unexpected);
  assert(errors.takeError()==ramses::BAD_CHECKSUM);
  assert(errors.takeError()==ramses::NO_REJECTION);
  auto badManchester=frame;
  badManchester[12]=0xF0;
  for(uint8_t byte:badManchester)errors.feed(byte,unexpected);
  assert(errors.takeError()==ramses::INVALID_SYMBOL);
  errors.reset();
  for(size_t i=0;i<10;i++)errors.feed(frame[i],unexpected);
  assert(errors.receiving());
  errors.abortIdleTimeout();
  assert(!errors.receiving());
  assert(errors.takeError()==ramses::IDLE_TIMEOUT);
  diag.rejection(ramses::BAD_CHECKSUM,300);
  diag.rejection(ramses::INVALID_SYMBOL,301);
  diag.rejection(ramses::IDLE_TIMEOUT,302);
  assert(diag.checksum==1 && diag.invalidSymbol==1 && diag.idleTimeout==1);
  assert(diag.lastReject==ramses::IDLE_TIMEOUT && diag.lastRejectMs==302);
  // Source address 10:048994 vs recipient 10:048994 remain distinct.
  ramses::Message bridgeRequest{};
  bridgeRequest.messageType=0;bridgeRequest.addrMask=0x03;
  bridgeRequest.addresses[0][0]=static_cast<uint8_t>(1<<2);
  bridgeRequest.addresses[1][0]=static_cast<uint8_t>(10<<2);
  bridgeRequest.addresses[1][1]=static_cast<uint8_t>(48994>>8);
  bridgeRequest.addresses[1][2]=static_cast<uint8_t>(48994);
  bridgeRequest.opcode=0x3EF0;
  diag.packet(bridgeRequest,48994,400);
  assert(diag.requestsToBridge==1 && diag.messagesFromBridge==0);
  ramses::Message bridgeReply=bridgeRequest;
  bridgeReply.messageType=3;
  for(int b=0;b<3;b++){
    bridgeReply.addresses[0][b]=bridgeRequest.addresses[1][b];
    bridgeReply.addresses[1][b]=bridgeRequest.addresses[0][b];
  }
  diag.packet(bridgeReply,48994,450);
  assert(diag.requestsToBridge==1 && diag.messagesFromBridge==1);
  assert(diag.bridge3ef0==1 && diag.lastBridgeMs==450);
  diag.packet(bridgeReply,0,451);
  assert(diag.messagesFromBridge==1);

  puts("RAMSES Manchester, checksum, RSSI, RX diagnostics and bridge-address tests PASS");
  return 0;
}
