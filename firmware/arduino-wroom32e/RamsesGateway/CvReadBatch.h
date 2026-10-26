// A bounded, manually authorized batch of three central-heating Read-Data queries.
// The radio task drives this model through the existing paired-bridge Probe.
// No writes, bind traffic, automatic retries, scans or flash persistence.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "RamsesPairedBridgeRead.h"
#include "OpenThermTxDiagnostics.h"

namespace cv_read_batch {
constexpr uint8_t IDS[]={18,117,121};
constexpr size_t COUNT=sizeof(IDS)/sizeof(IDS[0]);
constexpr uint32_t SPACING_MS=ramses_bridge_read::COOLDOWN_MS;
constexpr uint32_t BLOCK_TIMEOUT_MS=120000u;
struct Item {
  uint8_t id=0;
  ramses_bridge_read::State state=ramses_bridge_read::IDLE;
  uint16_t raw=0;
  uint32_t updatedAtMs=0;
  ot_tx::Phase txPhase=ot_tx::NOT_STARTED;
  uint16_t txBytes=0,txExpectedBytes=0;
};
struct Batch {
  Item items[COUNT]={};
  bool active=false,completed=false,aborted=false,inflight=false;
  uint32_t controllerId=0; // frozen observed 01 source for all three reads
  uint8_t next=0;
  uint32_t startedAtMs=0,completedAtMs=0,dueMs=0;
  bool start(uint32_t now,uint32_t displayId) {
    if(active || !displayId || displayId>=262143)return false;
    for(size_t i=0;i<COUNT;i++){items[i]=Item{};items[i].id=IDS[i];}
    active=true;completed=false;aborted=false;inflight=false;next=0;
    controllerId=displayId;
    startedAtMs=now;completedAtMs=0;dueMs=now;return true;
  }
  bool due(uint32_t now)const {
    return active&&!inflight&&next<COUNT &&
      static_cast<int32_t>(now-dueMs)>=0;
  }
  uint8_t id()const {return next<COUNT?IDS[next]:0;}
  void queued() {
    if(!active||inflight||next>=COUNT)return;
    inflight=true;
    items[next].state=ramses_bridge_read::QUEUED;
  }
  bool settle(const ramses_bridge_read::Probe& probe,
              const ot_tx::Report& tx,uint32_t now) {
    if(!active||!inflight||next>=COUNT || probe.dataId!=IDS[next] ||
       probe.busy())return false;
    Item& item=items[next];
    item.state=probe.state;item.raw=probe.raw;item.updatedAtMs=now;
    item.txPhase=tx.phase;
    item.txBytes=tx.written_bytes;
    item.txExpectedBytes=tx.expected_bytes;
    inflight=false;next++;
    dueMs=now+SPACING_MS;
    if(next==COUNT){active=false;completed=true;completedAtMs=now;}
    return true;
  }
  bool overdue(uint32_t now) const {
    return due(now) && static_cast<uint32_t>(now-dueMs)>BLOCK_TIMEOUT_MS;
  }
  void abort(uint32_t now) {
    if(!active)return;
    active=false;completed=true;aborted=true;inflight=false;
    completedAtMs=now;
  }
};
} // namespace cv_read_batch
