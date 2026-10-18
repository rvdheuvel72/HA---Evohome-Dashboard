// GPIO26 (UART1 TX) -> GPIO25 (UART2 RX) bench loopback, with NO CC1101 STX.
// Pure byte generation and comparison for host-native regression testing.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "RamsesBinding.h"
#include "RamsesReadRequest.h"

namespace ramses_loopback {
constexpr uint8_t TX_GPIO=26;
constexpr uint8_t RX_GPIO=25;
constexpr size_t MAX_WIRE=96;
// Only a local fixture: no RF/bridge packet is ever sent by this helper.
constexpr uint32_t FIXTURE_CONTROLLER_ID=160001;
constexpr uint32_t FIXTURE_BRIDGE_ID=48994;

enum State : uint8_t {
  NOT_RUN, QUEUED, RUNNING, PASS, NO_SIGNAL, SHORT_CAPTURE,
  DIFFERENT_BYTES, EXTRA_BYTES, UART_WRITE_FAILED, INVALID_FIXTURE,
  DECODE_FAILED,
  UNSAFE_RF_STATE, UART_START_FAILED
};
inline const char* name(State state) {
  switch(state) {
    case NOT_RUN:return "not_run";
    case QUEUED:return "queued";
    case RUNNING:return "running";
    case PASS:return "pass";
    case NO_SIGNAL:return "no_signal";
    case SHORT_CAPTURE:return "short_capture";
    case DIFFERENT_BYTES:return "different_bytes";
    case EXTRA_BYTES:return "extra_bytes";
    case UART_WRITE_FAILED:return "uart_write_failed";
    case INVALID_FIXTURE:return "invalid_fixture";
    case DECODE_FAILED:return "decode_failed";
    case UNSAFE_RF_STATE:return "unsafe_radio_state";
    case UART_START_FAILED:return "uart_start_failed";
  }
  return "unknown";
}
struct Report {
  State state=NOT_RUN;
  uint8_t expected[MAX_WIRE]={},captured[MAX_WIRE]={};
  uint16_t expected_len=0,captured_len=0,uart_written=0;
  int16_t first_difference=-1;
  uint32_t uart_elapsed_us=0, capture_elapsed_us=0, completed_ms=0;
  uint8_t radio_marc_before=0xff,radio_marc_after=0xff;
  bool decoded_1fc9=false;
};
inline bool makeFixture(uint8_t* out,size_t cap,size_t& len) {
  len=0;
  if(!out)return false;
  uint8_t frame[64]={};size_t frameLen=0;
  if(!ramses_bind::makeOffer(FIXTURE_CONTROLLER_ID,FIXTURE_BRIDGE_ID,frame,frameLen))
    return false;
  len=ramses_read::encodeUART(frame,frameLen,out,cap);
  return len==70;
}
inline State compare(Report& result) {
  result.first_difference=-1;
  if(!result.expected_len || result.expected_len>MAX_WIRE ||
     result.captured_len>MAX_WIRE)return INVALID_FIXTURE;
  const size_t common=result.expected_len<result.captured_len?
     result.expected_len:result.captured_len;
  for(size_t i=0;i<common;++i) {
    if(result.expected[i]!=result.captured[i]) {
      result.first_difference=static_cast<int16_t>(i);
      return DIFFERENT_BYTES;
    }
  }
  if(!result.captured_len)return NO_SIGNAL;
  if(result.captured_len<result.expected_len)return SHORT_CAPTURE;
  if(result.captured_len>result.expected_len)return EXTRA_BYTES;
  return PASS;
}
inline bool hexBytes(const uint8_t* src,size_t len,char* out,size_t cap) {
  if(!src || !out || len>MAX_WIRE || cap<2*len+1)return false;
  static constexpr char HEX[]="0123456789ABCDEF";
  for(size_t i=0;i<len;i++) {
    out[2*i]=HEX[src[i]>>4];
    out[2*i+1]=HEX[src[i]&15u];
  }
  out[2*len]=0;
  return true;
}
} // namespace ramses_loopback
