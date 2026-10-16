#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../RamsesGateway/RamsesPacketCapture.h"
int main(){
  // Static memory footprint and explicit end-of-capacity behavior.
  static ramses_capture::Session capture;
  assert(sizeof(ramses_capture::Entry)<=256);
  assert(sizeof(capture)<48000);
  assert(!capture.active && capture.count==0);
  assert(!capture.append(ramses_capture::PACKET,"RQ",1,80));
  capture.start(10,300,2,1);
  assert(capture.active && capture.start_uart_bytes==300);
  assert(capture.append(ramses_capture::PACKET,
    "092 RP --- 10:048994 01:245914 --:------ 3EF0 009 000010000000020A64",11,92));
  assert(capture.append(ramses_capture::REJECTION,
    "invalid_manchester raw_uart_byte=0xEF (sender unknown)",12,0));
  ramses_capture::Entry row;
  assert(capture.item(0,row) && row.uptime_ms==11 && row.rssi==92);
  assert(row.kind==ramses_capture::PACKET && row.sequence==1);
  assert(strstr(row.message,"10:048994")!=nullptr);
  assert(capture.item(1,row) && row.kind==ramses_capture::REJECTION);
  assert(!capture.item(2,row));
  capture.stop(13);
  assert(!capture.active && capture.stopped_ms==13);
  assert(!capture.append(ramses_capture::PACKET,"I",14,0));
  capture.start(20,400,3,2);
  assert(capture.count==0 && !capture.full);
  char longText[300];
  memset(longText,'A',sizeof(longText)-1);longText[sizeof(longText)-1]=0;
  for(size_t i=0;i<ramses_capture::MAX_ITEMS;i++){
    assert(capture.append(ramses_capture::PACKET,longText,
                          21+static_cast<uint32_t>(i),90));
  }
  assert(capture.full && !capture.active);
  assert(capture.count==ramses_capture::MAX_ITEMS);
  assert(capture.received==ramses_capture::MAX_ITEMS);
  assert(!capture.append(ramses_capture::PACKET,"EXCESS",300,90));
  assert(capture.item(capture.count-1,row));
  assert(row.message[ramses_capture::MAX_TEXT-1]==0);
  capture.clear();
  assert(capture.count==0 && !capture.full && !capture.active);
  puts("RAMSES bounded capture, rejection events, start/stop/clear and full tests PASS");
}
