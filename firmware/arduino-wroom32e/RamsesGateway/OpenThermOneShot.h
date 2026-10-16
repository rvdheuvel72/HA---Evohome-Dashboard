// Explicitly authorized ONE OpenTherm Read-Data ID 25 request.
// No retries, no scan. Host-unit-testable without an ESP32.
#pragma once
#include <stdint.h>
#include "OpenThermReadOnly.h"
#include "OpenThermTxDiagnostics.h"
namespace ot_single {
constexpr uint8_t DATA_ID=25;
constexpr uint32_t TIMEOUT_MS=1800;
constexpr uint32_t COOLDOWN_MS=60000;
enum State : uint8_t { IDLE, QUEUED, WAITING, READ_ACK, DATA_INVALID,
                       UNKNOWN_ID, TIMEOUT, TX_FAILED };
struct Probe {
  State state=IDLE;
  ot_tx::Method method=ot_tx::ASYNC_UART;
  uint32_t lastAttemptMs=0,deadlineMs=0;
  bool attempted=false;
  uint16_t raw=0;
  bool request(uint32_t now,ot_tx::Method choice=ot_tx::ASYNC_UART) {
    if(state==QUEUED || state==WAITING || cooldown(now)!=0)return false;
    if(choice!=ot_tx::ASYNC_UART && choice!=ot_tx::REFERENCE_FIFO)return false;
    state=QUEUED;method=choice;raw=0;return true;
  }
  bool begin(uint32_t now) {
    if(state!=QUEUED)return false;
    state=WAITING;attempted=true;lastAttemptMs=now;
    deadlineMs=now+TIMEOUT_MS;return true;
  }
  void sent(bool ok) {
    if(state==WAITING && !ok)state=TX_FAILED;
  }
  bool accept(uint8_t id,ot_readonly::Result result,uint16_t value,uint32_t now) {
    if(state!=WAITING || id!=DATA_ID || static_cast<int32_t>(now-deadlineMs)>=0)return false;
    switch(result) {
      case ot_readonly::READ_ACK:state=READ_ACK;break;
      case ot_readonly::DATA_INVALID:state=DATA_INVALID;break;
      case ot_readonly::UNKNOWN_ID:state=UNKNOWN_ID;break;
      default:return false;
    }
    raw=value;return true;
  }
  void tick(uint32_t now) {
    if(state==WAITING && static_cast<int32_t>(now-deadlineMs)>=0)state=TIMEOUT;
  }
  uint32_t cooldown(uint32_t now) const {
    if(!attempted)return 0;
    const uint32_t elapsed=now-lastAttemptMs;
    return elapsed>=COOLDOWN_MS?0:COOLDOWN_MS-elapsed;
  }
  const char* label() const {
    switch(state) {
      case IDLE:return "not_tested";case QUEUED:return "queued";
      case WAITING:return "awaiting_reply";case READ_ACK:return "read_ack";
      case DATA_INVALID:return "data_invalid";
      case UNKNOWN_ID:return "unknown_data_id";
      case TIMEOUT:return "timeout";case TX_FAILED:return "tx_failed";
    }
    return "unknown";
  }
};
} // namespace ot_single
