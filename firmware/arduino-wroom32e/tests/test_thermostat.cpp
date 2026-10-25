#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../RamsesGateway/RamsesThermostat.h"
using namespace ramses_thermostat;
int main(){
  uint8_t frame[64]={};size_t n=0;
  assert(makeOffer(10740,145038,frame,n) && n==35);
  ramses::Message offer{};
  assert(ramses::decode(frame,n,offer));
  assert(offer.messageType==1&&offer.addrMask==5&&offer.opcode==0x1FC9&&offer.length==24);
  const uint8_t expectedOffer[]={
    0x18,0x30,0x29,0xF4,0x30,0x29,0xF4,0x1F,0xC9,0x18,
    0x00,0x23,0x09,0x30,0x29,0xF4,
    0x00,0x30,0xC9,0x30,0x29,0xF4,
    0x00,0x00,0x08,0x30,0x29,0xF4,
    0x00,0x1F,0xC9,0x30,0x29,0xF4
  };
  assert(memcmp(frame,expectedOffer,sizeof(expectedOffer))==0);
  assert(!makeOffer(0,145038,frame,n));
  assert(!makeOffer(10740,0,frame,n));
  assert(makeConfirmation(10740,145038,8,frame,n)&&n==17);
  ramses::Message confirmation{};
  assert(ramses::decode(frame,n,confirmation));
  assert(confirmation.addrMask==3&&confirmation.opcode==0x1FC9);
  // Golden type-12 capture in ramses_rf code_1fc9.log: W assigns
  // zone 08, but type-12 confirmation explicitly uses index 00.
  assert(confirmation.payload[0]==0 && confirmation.payload[1]==0x23&&confirmation.payload[2]==0x09);
  assert(type(confirmation.addresses[0])==12);
  assert(type(confirmation.addresses[1])==1);
  assert(confirmation.addresses[0][0]==0x30 &&
         confirmation.addresses[0][1]==0x29 &&
         confirmation.addresses[0][2]==0xF4);
  // Independent, real 12:010740 30C9 packet in code_30c9.log:
  // I --- 12:010740 --:------ 12:010740 30C9 003 000785.
  assert(makeTemperature(10740,145038,8,1925,frame,n)&&n==14);
  ramses::Message golden{};
  assert(ramses::decode(frame,n,golden));
  assert(golden.messageType==1 && golden.addrMask==5);
  assert(type(golden.addresses[0])==12 &&
         type(golden.addresses[2])==12);
  assert(id(golden.addresses[0])==10740 &&
         id(golden.addresses[2])==10740);
  assert(golden.payload[0]==0 &&
         golden.payload[1]==0x07 && golden.payload[2]==0x85);
  assert(makeTemperature(10740,145038,8,2154,frame,n)&&n==14);
  ramses::Message temp{};
  assert(ramses::decode(frame,n,temp)&&temp.opcode==0x30C9);
  assert(temp.messageType==1 && temp.addrMask==5);
  assert(temp.length==3&&temp.payload[0]==0&&temp.payload[1]==0x08&&temp.payload[2]==0x6A);
  assert(!makeTemperature(10740,145038,8,9000,frame,n));
  assert(!makeTemperature(10740,145038,12,2100,frame,n));
  assert(makeTemperature(10740,145038,8,-500,frame,n));
  assert(ramses::decode(frame,n,temp));
  assert(temp.payload[1]==0xFE&&temp.payload[2]==0x0C);
  ramses::Message acceptance{};
  acceptance.messageType=2;acceptance.addrMask=3;acceptance.opcode=0x1FC9;
  acceptance.length=6;
  ramses_read::putAddress(acceptance.addresses[0],1,145038);
  ramses_read::putAddress(acceptance.addresses[1],12,10740);
  acceptance.payload[0]=8;acceptance.payload[1]=0x23;acceptance.payload[2]=0x09;
  ramses_read::putAddress(acceptance.payload+3,1,145038);
  // PubSubClient does not expose MQTT RETAIN to its callback. Timestamped
  // measurements retain their ORIGINAL age across reconnect replays.
  int32_t mqttTemp=0;uint32_t mqttAge=0;
  constexpr uint32_t epoch=1792929000u;
  assert(mqttStampedTemperature("1792928940:2150",15,epoch,mqttTemp,mqttAge));
  assert(mqttTemp==2150 && mqttAge==60000u);
  assert(mqttStampedTemperature("1792928940:-500",15,epoch,mqttTemp,mqttAge));
  assert(mqttTemp==-500 && mqttAge==60000u);
  assert(!mqttStampedTemperature("2150",4,epoch,mqttTemp,mqttAge)); // no timestamp
  assert(!mqttStampedTemperature("1792928099:2150",15,epoch,mqttTemp,mqttAge)); // 901s old
  assert(!mqttStampedTemperature("1792929031:2150",15,epoch,mqttTemp,mqttAge)); // future
  assert(!mqttStampedTemperature("1792928940:9999",15,epoch,mqttTemp,mqttAge));
  assert(!mqttStampedTemperature("1792928940:21x0",15,epoch,mqttTemp,mqttAge));
  // A broker reconnect at +850 seconds must not reset the original sample
  // lifetime: the same retained value then expires at +900 seconds.
  assert(mqttStampedTemperature("1792928940:2150",15,epoch+839u,mqttTemp,mqttAge));
  assert(mqttAge==899000u);
  assert(!mqttStampedTemperature("1792928940:2150",15,epoch+840u,mqttTemp,mqttAge));
  Session s;
  assert(s.request(1000,10740,145038));
  assert(!s.request(1001,10740,145038));
  s.offered(true,1010);
  ramses::Message wrong=acceptance;
  wrong.payload[1]=0x30;
  assert(!s.observe(wrong,1020));
  wrong=acceptance;
  ramses_read::putAddress(wrong.addresses[0],1,145039);
  assert(!s.observe(wrong,1020));
  wrong=acceptance;
  ramses_read::putAddress(wrong.addresses[1],12,10741);
  assert(!s.observe(wrong,1020));
  assert(s.observe(acceptance,1030));
  assert(s.state==CONFIRM_QUEUED&&s.accepted&&s.zone==8);
  s.confirmationSent(true);
  assert(s.state==PAIRED&&s.confirmed);
  assert(!s.reportDue(1040));
  assert(s.setTemperature(2154,1040));
  assert(s.reportDue(1040));
  s.reported(true,1040);
  assert(!s.reportDue(1041));
  assert(s.reportDue(301040));
  assert(!s.reportDue(901041)); // value stale after 15 min
  s.tick(901041);
  assert(!s.hasTemp);
  // Simulate full millis() rollover: an expired sample must not revive.
  assert(!s.reportDue(1040));
  assert(!s.hasTemp);
  // Wrapping across the initial, *fresh* sample time is still supported.
  assert(s.setTemperature(2250,0xfffffff0u));
  assert(s.temperatureFresh(1000u));
  s.tick(1000u);
  assert(s.hasTemp);
  s.tick(static_cast<uint32_t>(0xfffffff0u+ramses_thermostat::TEMPERATURE_TTL_MS));
  assert(!s.hasTemp && !s.reportDue(0xfffffff0u));
  assert(!s.setTemperature(9999,901100));
  assert(!s.reportDue(901100));
  assert(s.setTemperature(2050,901100));
  assert(s.reportDue(901100));
  // Stopping a paired 12: device invalidates temperature and all future
  // reports, while retaining the RF retry cooldown.
  const uint32_t beforeForget=s.cooldown(901101);
  s.forget();
  assert(s.state==IDLE && s.thermostatId==0 && s.controllerId==0);
  assert(!s.reportDue(901102) && !s.hasTemp && !s.accepted);
  assert(!s.observe(acceptance,901102));
  assert(s.cooldown(901101)==beforeForget);
  assert(s.request(901102,10740,145038)); // first session cooldown elapsed
  s.forget();
  assert(!s.request(901103,10740,145038)); // new cooldown preserved
  assert(s.request(1000000,10740,145038));
  s.offered(true,1000001);
  s.forget();
  assert(s.state==IDLE);
  assert(!s.observe(acceptance,1000002));
  Session late;
  assert(late.request(200000,10740,145038));
  late.offered(true,200001);
  assert(!late.observe(acceptance,215001));
  assert(late.state==NO_RESPONSE);
  Session collision;
  assert(collision.request(400000,10740,145038));
  collision.offered(true,400001);
  ramses::Message spoof{};
  spoof.addrMask=1;spoof.messageType=1;
  ramses_read::putAddress(spoof.addresses[0],12,10740);
  assert(!collision.observe(spoof,400002));
  assert(collision.state==ID_CONFLICT&&!collision.reportDue(400005));
  Session failed;
  assert(failed.request(500000,10740,145038));
  failed.offered(false,500001);
  assert(failed.state==TX_FAILED);
  puts("Virtual 12: thermostat binding, temperature, TTL, cooldown and collision tests PASS");
  return 0;
}
