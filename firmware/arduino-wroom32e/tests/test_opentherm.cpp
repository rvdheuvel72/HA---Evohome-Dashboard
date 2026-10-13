#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../RamsesGateway/OpenThermReadOnly.h"
#include "../RamsesGateway/OpenThermScan.h"
#include "../RamsesGateway/RamsesReadRequest.h"
#include "../RamsesGateway/RadioFrequency.h"
#include "../RamsesGateway/RamsesCodec.h"

static void reply(uint8_t type,uint8_t id,uint16_t value,uint8_t out[5]){
  out[0]=0;
  out[1]=static_cast<uint8_t>(type<<4);
  out[2]=id;
  out[3]=static_cast<uint8_t>(value>>8);
  out[4]=static_cast<uint8_t>(value);
  uint32_t v=(static_cast<uint32_t>(out[1])<<24)|
             (static_cast<uint32_t>(id)<<16)|value;
  if(ot_readonly::parity(v))out[1]|=0x80;
}
int main(){
  uint32_t freq=0;
  assert(radio_frequency::synth(868300,868,freq));
  assert(freq==0x21656A);
  assert(!radio_frequency::synth(433920,868,freq));
  assert(radio_frequency::synth(433920,433,freq));
  assert(!radio_frequency::synth(867000,0,freq));
  assert(radio_frequency::evohomeTxFrequency(868300,868));
  assert(!radio_frequency::evohomeTxFrequency(868350,868));
  assert(!radio_frequency::evohomeTxFrequency(868300,915));
  uint8_t req[5]={};
  assert(ot_readonly::request(25,req));
  const uint8_t known[]={0x00,0x80,0x19,0x00,0x00};
  assert(memcmp(req,known,sizeof(req))==0);
  assert(ot_readonly::request(17,req));
  assert(req[1]==0 && req[2]==17);
  assert(ot_readonly::request(128,req));
  assert(ot_readonly::request(255,req));
  assert(!ot_readonly::request(256,req));
  uint8_t rawMsg[16]={};
  size_t rawLength=0;
  assert(ramses_read::makeRequest(100001,123456,25,rawMsg,rawLength));
  assert(rawLength==16 && rawMsg[7]==0x32 && rawMsg[8]==0x20);
  ramses::Message decoded{};
  assert(ramses::decode(rawMsg,rawLength,decoded));
  assert(decoded.opcode==0x3220 && decoded.messageType==0);
  assert(decoded.length==5 && memcmp(decoded.payload,known,5)==0);
  assert(!ramses_read::makeRequest(730,123456,25,rawMsg,rawLength));
  assert(!ramses_read::makeRequest(100001,0,25,rawMsg,rawLength));
  assert(ramses_read::makeRequest(100001,123456,25,rawMsg,rawLength));
  uint8_t uart[44]={};
  assert(ramses_read::encodeUART(rawMsg,rawLength,uart,sizeof(uart))==44);
  assert(uart[0]==0x55 && uart[5]==0xff && uart[6]==0x00);
  assert(uart[7]==0x33 && uart[8]==0x55 && uart[9]==0x53);
  assert(uart[42]==0x35 && uart[43]==0x55);
  ramses::FrameDecoder rx;
  ramses::Message received{};
  bool found=false;
  for (uint8_t b:uart) if(rx.feed(b,received))found=true;
  assert(found && received.opcode==0x3220);
  uint8_t response[5]={};uint8_t id=0;uint16_t raw=0;
  ot_readonly::Result type=ot_readonly::NOT_TESTED;
  reply(4,25,0x2580,response);
  assert(ot_readonly::response(response,5,id,type,raw));
  assert(id==25 && type==ot_readonly::READ_ACK && raw==0x2580);
  float value=0;
  assert(ot_readonly::fixedPoint(id,raw,value));
  assert(value==37.5f);
  response[3]^=1;
  assert(!ot_readonly::response(response,5,id,type,raw)); // parity
  reply(6,26,0,response);
  assert(ot_readonly::response(response,5,id,type,raw) &&
         type==ot_readonly::DATA_INVALID);
  reply(7,19,0,response);
  assert(ot_readonly::response(response,5,id,type,raw) &&
         type==ot_readonly::UNKNOWN_ID);
  reply(5,25,0,response); // Write-Ack must never be treated as diagnostic
  assert(!ot_readonly::response(response,5,id,type,raw));
  assert(ot_readonly::pollable(25));
  assert(!ot_readonly::pollable(115));
  ot_scan::Scanner scan;
  scan.reset(123456,false,1000);
  assert(scan.nextRequest(1000)==0);
  assert(scan.nextRequest(2000)==-1);
  assert(scan.accept(0,ot_readonly::READ_ACK,1,2000));
  assert(!scan.accept(25,ot_readonly::READ_ACK,1,2100));
  assert(scan.nextRequest(5000)==-1); // response cooldown
  assert(scan.nextRequest(7000)==3);
  assert(scan.nextRequest(8800)==-1); // records timeout, now cooldown
  assert(scan.entries[3].result==ot_readonly::TIMEOUT);
  assert(scan.nextRequest(13800)==5);
  scan.accept(5,ot_readonly::UNKNOWN_ID,0,13850);
  assert(scan.entries[5].result==ot_readonly::UNKNOWN_ID);
  ot_scan::Scanner fullScan;
  uint32_t clockMs=100;
  fullScan.reset(123456,true,clockMs);
  for (unsigned index=0;index<256;index++) {
    assert(fullScan.nextRequest(clockMs)==static_cast<int>(index));
    assert(fullScan.accept(static_cast<uint8_t>(index),ot_readonly::UNKNOWN_ID,0,clockMs+1));
    clockMs+=ot_scan::PROBE_SPACING_MS+2;
  }
  assert(fullScan.nextRequest(clockMs)==-1);
  assert(fullScan.completed && fullScan.attempted==256);
  printf("OpenTherm read-only encoding, parity, state machine, frequency and RAMSES roundtrip PASS\n");
  return 0;
}
