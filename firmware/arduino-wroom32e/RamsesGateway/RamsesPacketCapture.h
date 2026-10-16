// Bounded, session-oriented RAMSES receive capture. Host-testable.
// No RF TX, flash writes, dynamic allocation or network activity.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace ramses_capture {
constexpr size_t MAX_ITEMS=192;       // ~45 KiB DRAM on ESP32-WROOM
constexpr size_t MAX_TEXT=224;        // Full RAMSES payload up to 64 bytes
enum Kind : uint8_t { PACKET=0, REJECTION=1 };
inline const char* kindName(Kind k) {return k==PACKET?"packet":"rejected";}
struct Entry {
  uint32_t uptime_ms=0;
  uint64_t utc_ms=0; // Time when radio task decoded/rejected event, 0 if not synced
  uint32_t sequence=0;
  uint8_t rssi=0;
  Kind kind=PACKET;
  char message[MAX_TEXT]={};
};
struct Session {
  Entry entries[MAX_ITEMS]={};
  size_t count=0;
  uint32_t started_ms=0,stopped_ms=0;
  uint32_t start_uart_bytes=0,start_syncs=0,start_dropped=0;
  uint32_t received=0,rejected=0;
  bool active=false,full=false;
  void start(uint32_t now,uint32_t uartBytes,uint32_t syncs,uint32_t drops) {
    count=0;started_ms=now;stopped_ms=0;
    start_uart_bytes=uartBytes;start_syncs=syncs;start_dropped=drops;
    received=0;rejected=0;full=false;active=true;
  }
  void stop(uint32_t now){if(active){active=false;stopped_ms=now;}}
  void clear() {
    count=0;received=0;rejected=0;
    active=false;full=false;started_ms=0;stopped_ms=0;
    start_uart_bytes=0;start_syncs=0;start_dropped=0;
  }
  bool append(Kind kind,const char* text,uint32_t now,uint8_t rssi,
              uint64_t utcMs=0) {
    if(!active || !text)return false;
    if(count>=MAX_ITEMS){active=false;full=true;stopped_ms=now;return false;}
    Entry& entry=entries[count];
    entry.uptime_ms=now;
    entry.utc_ms=utcMs;
    entry.sequence=static_cast<uint32_t>(count+1);
    entry.rssi=rssi;
    entry.kind=kind;
    // Explicitly indicate source truncation in the downloadable capture.
    const size_t length=strlen(text);
    const size_t copied=length<MAX_TEXT?length:MAX_TEXT-1;
    memcpy(entry.message,text,copied);
    entry.message[copied]='\0';
    if(kind==PACKET)received++; else rejected++;
    count++;
    if(count==MAX_ITEMS){active=false;full=true;stopped_ms=now;}
    return true;
  }
  bool item(size_t index,Entry& output)const{
    if(index>=count)return false;
    output=entries[index];
    return true;
  }
};
} // namespace ramses_capture
