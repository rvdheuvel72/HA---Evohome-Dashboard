// Deterministic and Arduino-independent read-only OpenTherm compatibility scanner.
// Operates on a single 3220 RQ at a time, never uses RAMSES W or OpenTherm Write-Data.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "OpenThermReadOnly.h"
namespace ot_scan {
static constexpr uint32_t PROBE_SPACING_MS=5000;
static constexpr uint32_t REPLY_TIMEOUT_MS=1800;
static constexpr uint16_t NONE=256;
struct Scanner {
  ot_readonly::Entry entries[256] = {};
  bool active=false;
  bool completed=false;
  bool full=false;
  uint16_t pending=NONE;
  uint16_t next=0;
  uint16_t attempted=0;
  uint16_t acknowledged=0;
  uint32_t deadline=0;
  uint32_t due=0;
  uint32_t completedAtMs=0;
  uint32_t bridgeId=0;

  void reset(uint32_t bridge,bool all,uint32_t now) {
    for (size_t i=0;i<256;i++) entries[i]=ot_readonly::Entry{};
    bridgeId=bridge;active=true;completed=false;full=all;
    pending=NONE;next=0;attempted=0;acknowledged=0;
    deadline=0;due=now;completedAtMs=0;
  }
  void finish(uint32_t now) {
    active=false;completed=true;pending=NONE;completedAtMs=now;
  }
  // Returns a Data-ID to transmit, or -1 if waiting. Caller gates actual
  // radio transmission and must cancel on hardware faults.
  int nextRequest(uint32_t now) {
    if(!active)return -1;
    if(pending!=NONE){
      if(static_cast<int32_t>(now-deadline)<0)return -1;
      entries[pending].result=ot_readonly::TIMEOUT;
      pending=NONE;
      due=now+PROBE_SPACING_MS;
    }
    if(static_cast<int32_t>(now-due)<0)return -1;
    const size_t count=full?256:ot_readonly::DEFAULT_COUNT;
    if(next>=count){finish(now);return -1;}
    const uint8_t id=full?static_cast<uint8_t>(next):ot_readonly::DEFAULT_IDS[next];
    next++;
    pending=id;attempted++;
    deadline=now+REPLY_TIMEOUT_MS;
    return id;
  }
  bool accept(uint8_t id,ot_readonly::Result r,uint16_t raw,uint32_t now) {
    if(!active || pending!=id || r==ot_readonly::NOT_TESTED || r==ot_readonly::TIMEOUT)
      return false;
    entries[id].result=r;
    entries[id].raw=raw;
    entries[id].receivedAtMs=now;
    if(r==ot_readonly::READ_ACK)acknowledged++;
    pending=NONE;
    due=now+PROBE_SPACING_MS;
    return true;
  }
  // A finished scan is a snapshot of replies, NOT proof of permanently
  // supported IDs. UNKNOWN and TIMEOUT are not the same condition.
  bool autoPoll(uint8_t id) const {
    return completed && entries[id].result==ot_readonly::READ_ACK &&
       ot_readonly::pollable(id);
  }
};
} // namespace ot_scan
