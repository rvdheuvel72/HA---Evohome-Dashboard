#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "../RamsesGateway/OpenThermReadOnly.h"
#include "../RamsesGateway/OpenThermScan.h"
#include "../RamsesGateway/OpenThermOneShot.h"
#include "../RamsesGateway/RamsesUartFifo.h"
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
// Independent host-side model of IndaloTech/ramses_esp frame.c tx_byte(),
// including the original 16-bit little-endian union, variable bit phase,
// four initial FIFO primer bytes and final idle MARK. Do NOT call our encoder.
static std::vector<uint8_t> upstreamReferenceBits(const uint8_t* bytes,size_t count) {
  std::vector<uint8_t> out={0x00,0xFF,0x00,0x00};
  uint16_t reg=0;
  unsigned txBits=0;
  const auto reverse8=[](uint8_t v)->uint8_t {
    uint8_t result=0;
    for(unsigned b=0;b<8;b++)result|=static_cast<uint8_t>(((v>>b)&1u)<<(7u-b));
    return result;
  };
  const auto shift=[&](unsigned n){reg=static_cast<uint16_t>(reg<<n);};
  const auto output=[&](){out.push_back(static_cast<uint8_t>(reg>>8));};
  const auto insertPs=[&](){
    uint8_t hi=static_cast<uint8_t>(reg>>8);
    hi=static_cast<uint8_t>((hi<<1)|1u); // MARK
    hi=static_cast<uint8_t>(hi<<1);      // SPACE
    reg=static_cast<uint16_t>((reg&0xffu)|(static_cast<uint16_t>(hi)<<8));
  };
  for(size_t i=0;i<count;i++){
    reg=static_cast<uint16_t>((reg&0xff00u)|reverse8(bytes[i]));
    switch(txBits) {
      case 0:
        insertPs();shift(6);output();shift(2);txBits=2;break;
      case 2:
        insertPs();shift(4);output();shift(4);txBits=4;break;
      case 4:
        insertPs();shift(2);output();shift(6);
        insertPs();output();txBits=8;break;
      case 6:
        insertPs();output();txBits=8;break;
      case 8:
        shift(8);output();txBits=0;break;
      default:assert(false);
    }
  }
  if(txBits){shift(8-txBits);output();}
  out.push_back(0xff);
  return out;
}

int main(){
  uint32_t freq=0;
  assert(radio_frequency::synth(868300,868,freq));
  assert(freq==0x21656A);
  assert(!radio_frequency::synth(433920,868,freq));
  assert(radio_frequency::synth(868300,433,freq,true)); // explicit RX opt-in
  assert(freq==0x21656A);
  assert(!radio_frequency::synth(868300,433,freq));
  assert(!radio_frequency::synth(500000,433,freq,true)); // outside silicon range
  assert(!radio_frequency::evohomeTxFrequency(868300,433));
  assert(radio_frequency::experimentalOneShotFrequency(868300,433,true));
  assert(!radio_frequency::experimentalOneShotFrequency(868300,433,false));
  assert(!radio_frequency::experimentalOneShotFrequency(868300,868,true));
  assert(!radio_frequency::experimentalOneShotFrequency(868350,433,true));
  assert(radio_frequency::synth(433920,433,freq));
  assert(!radio_frequency::synth(867000,0,freq));
  assert(radio_frequency::evohomeTxFrequency(868300,868));
  assert(!radio_frequency::evohomeTxFrequency(868350,868));
  assert(!radio_frequency::evohomeTxFrequency(868300,915));
  // P2: only a VERIFIED 01: binding allows a guarded one-shot read on
  // the standard 868-MHz hardware path, never on arbitrary RF tunings.
  assert(!radio_frequency::manualReadOneShotAllowed(868300,868,false,false));
  assert(radio_frequency::manualReadOneShotAllowed(868300,868,false,true));
  assert(!radio_frequency::manualReadOneShotAllowed(868350,868,false,true));
  assert(!radio_frequency::manualReadOneShotAllowed(433920,433,false,true));
  assert(!radio_frequency::manualReadOneShotAllowed(868300,433,false,false));
  assert(radio_frequency::manualReadOneShotAllowed(868300,433,true,false));
  assert(radio_frequency::manualReadOneShotAllowed(868300,433,true,true));
  // P2: a verified additional 01: controller is independent of the 18:
  // diagnostic-gateway source ID and its address-collision status.
  assert(radio_frequency::manualReadOneShotWithIdentity(
      868300,868,false,true,0,false));
  assert(radio_frequency::manualReadOneShotWithIdentity(
      868300,868,false,true,0,true));
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868300,868,false,false,0,false));
  // Unpaired 18: ID25 is permitted on a real 868 MHz module only when
  // the caller has also met the manual one-shot, bridge and TX-off guards.
  assert(radio_frequency::manualReadOneShotWithIdentity(
      868300,868,false,false,62142,false));
  assert(radio_frequency::manualReadOneShotWithIdentity(
      868300,868,true,false,62142,false));
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868300,868,false,false,62142,true)); // observed address collision
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868300,868,false,false,730,false)); // reserved source
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868300,868,false,false,262143,false)); // out of range
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868350,868,false,false,62142,false)); // wrong frequency
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868300,915,false,false,62142,false)); // wrong hardware
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868300,433,false,false,62142,false)); // cross-band opt-in missing
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868300,433,true,false,0,false));
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868300,433,true,false,730,false));
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868300,433,true,false,62142,true));
  assert(radio_frequency::manualReadOneShotWithIdentity(
      868300,433,true,false,62142,false));
  assert(radio_frequency::manualReadOneShotWithIdentity(
      868300,433,true,true,0,false));
  assert(!radio_frequency::manualReadOneShotWithIdentity(
      868350,868,false,true,0,false));
  // A manual click only queues one fixed Data-ID 25 request.
  ot_single::Probe shot;
  assert(shot.state==ot_single::IDLE && shot.request(10000,ot_tx::REFERENCE_FIFO));
  assert(shot.method==ot_tx::REFERENCE_FIFO);
  assert(strcmp(ot_tx::methodName(shot.method),"fifo_reference_primed_60")==0);
  assert(strcmp(ot_tx::phaseName(ot_tx::TX_UNDERFLOW),"tx_underflow")==0);
  assert(ot_tx::Report::PA_868_LOW==0x26);
  assert(ot_tx::Report::MAX_FIFO_TX_US==80000);
  ot_tx::Report report;
  assert(report.phase==ot_tx::NOT_STARTED);
  report.method=ot_tx::REFERENCE_FIFO;
  report.expected_bytes=44;
  report.written_bytes=44;
  report.phase=ot_tx::LOCAL_COMPLETE;
  assert(report.expected_bytes==report.written_bytes);
  ot_tx::Comparison foo;
  foo.attempts++;foo.local_completed++;
  assert(foo.attempts==1 && foo.local_completed==1);
  ot_single::Probe invalid;
  assert(!invalid.request(9999,static_cast<ot_tx::Method>(2)));
  assert(invalid.state==ot_single::IDLE);
  assert(!shot.request(10000));  // no double-click duplicate
  assert(shot.begin(10000));
  assert(!shot.begin(10000));
  assert(!shot.accept(28,ot_readonly::READ_ACK,0x2580,10020)); // wrong ID
  assert(!shot.accept(25,ot_readonly::NOT_TESTED,0x2580,10020));
  assert(shot.accept(25,ot_readonly::READ_ACK,0x2580,10020));
  assert(shot.state==ot_single::READ_ACK && shot.raw==0x2580);
  assert(!shot.request(60000)); // less than 60s since TX
  assert(shot.request(70000,ot_tx::ASYNC_UART) && shot.begin(70000));
  assert(shot.method==ot_tx::ASYNC_UART);
  shot.tick(71800); // no response, no retry, terminal timeout
  assert(shot.state==ot_single::TIMEOUT);
  assert(!shot.request(71900));
  assert(shot.request(130000) && shot.begin(130000));
  shot.sent(false);
  assert(shot.state==ot_single::TX_FAILED);
  assert(!shot.accept(25,ot_readonly::READ_ACK,0x2580,130010));
  assert(shot.request(190000) && shot.begin(190000));
  assert(!shot.accept(25,ot_readonly::READ_ACK,0x2580,191800));
  shot.tick(191800);
  assert(shot.state==ot_single::TIMEOUT);
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
  // Same RF bits as 38,400-baud serial UART: 44 x (start + 8 data + stop).
  uint8_t fifoFrame[55]={};
  size_t fifoLength=0;
  assert(ramses_uart_fifo::encode(uart,sizeof(uart),fifoFrame,sizeof(fifoFrame),fifoLength));
  assert(fifoLength==55);
  for(size_t n=0;n<44;n++) {
    const size_t start=n*10;
    auto rfBit=[&](size_t i)->uint8_t {
      return static_cast<uint8_t>((fifoFrame[i>>3]>>(7-(i&7)))&1u);
    };
    assert(rfBit(start)==0);
    for(unsigned b=0;b<8;b++)
      assert(rfBit(start+1+b)==((uart[n]>>b)&1u));
    assert(rfBit(start+9)==1);
  }
  // The upstream RAMSES FIFO TX primes with 00 FF 00 00, then shifts
  // the same 55-byte 8N1 waveform, and flushes a final FF idle MARK.
  // It must remain one bounded packet inside the 64-byte CC1101 FIFO.
  static_assert(ramses_uart_fifo::REFERENCE_RF_LEN==60,"FIFO capacity");
  uint8_t refFifo[64]={};
  size_t refLength=0;
  assert(ramses_uart_fifo::encodePrimedReference(
    uart,sizeof(uart),refFifo,sizeof(refFifo),refLength));
  assert(refLength==60);
  const uint8_t prime[]={0x00,0xFF,0x00,0x00};
  assert(memcmp(refFifo,prime,sizeof(prime))==0);
  // Before beta.8 our TX incorrectly matched START/DATA/STOP (0 8 1).
  // The upstream wire sequence is MARK/START/DATA (1 0 8).
  assert(memcmp(refFifo+sizeof(prime),fifoFrame,sizeof(fifoFrame))!=0);
  const auto golden=upstreamReferenceBits(uart,sizeof(uart));
  assert(golden.size()==ramses_uart_fifo::REFERENCE_RF_LEN);
  assert(memcmp(refFifo,golden.data(),golden.size())==0);
  // Five UART training 0x55 bytes give 0xAA reference FIFO octets:
  // the difference at the very first bit after the BREAK is observable.
  assert(refFifo[4]==0xAA && fifoFrame[0]==0x55);
  assert(refFifo[refLength-1]==0xFF);
  size_t primedInvalid=99;
  assert(!ramses_uart_fifo::encodePrimedReference(
    uart,43,refFifo,sizeof(refFifo),primedInvalid));
  assert(primedInvalid==0);
  assert(!ramses_uart_fifo::encodePrimedReference(
    uart,sizeof(uart),refFifo,59,primedInvalid));
  assert(primedInvalid==0);
  // Priming does not change any Manchester RAMSES data or introduce
  // another RQ message. Verify reconstructing UART octets from RF bits.
  uint8_t restored[44]={};
  for(size_t n=0;n<44;n++){
    const size_t start=(sizeof(prime)*8)+n*10;
    const auto bit=[&](size_t offset)->uint8_t {
      return (refFifo[offset/8]>>(7-(offset%8)))&1u;
    };
    assert(bit(start)==1 && bit(start+1)==0);
    for(unsigned b=0;b<8;b++)
      restored[n]|=static_cast<uint8_t>(bit(start+2+b)<<b);
  }
  assert(memcmp(restored,uart,sizeof(uart))==0);
  ramses::FrameDecoder referenceDecoder;
  ramses::Message referenceMsg{};
  unsigned referenceMessages=0;
  for(uint8_t c:restored)
    if(referenceDecoder.feed(c,referenceMsg))referenceMessages++;
  assert(referenceMessages==1 && referenceMsg.opcode==0x3220);
  size_t invalidLength=99;
  assert(!ramses_uart_fifo::encode(uart,43,fifoFrame,sizeof(fifoFrame),invalidLength));
  assert(invalidLength==0);
  assert(!ramses_uart_fifo::encode(uart,44,fifoFrame,54,invalidLength));
  assert(invalidLength==0);
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
  // OpenTherm ID 33 is signed 16-bit exhaust temperature in whole °C.
  assert(ot_readonly::fixedPoint(33,static_cast<uint16_t>(100),value));
  assert(value==100.0f);
  assert(ot_readonly::fixedPoint(33,static_cast<uint16_t>(-7),value));
  assert(value==-7.0f);
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
