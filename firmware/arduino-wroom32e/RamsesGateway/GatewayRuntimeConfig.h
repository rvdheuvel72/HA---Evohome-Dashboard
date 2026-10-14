// Settings overlay for GatewaySecrets.h. The header supplies factory defaults;
// a validated, atomically stored NVS record overrides them after first flash.
// Nothing edits the compiled header on-device.
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <stdint.h>
#include "GatewayHostname.h"
#include "RadioFrequency.h"

namespace gw_config {
inline bool safeText(const String& s, size_t maxLen) {
  if(s.length() >= maxLen) return false;
  for(size_t i=0;i<s.length();++i) {
    const unsigned char c=static_cast<unsigned char>(s[i]);
    if(c<32 || c==127) return false;
  }
  return true;
}
inline bool validTimeZone(const String& z) {
  return z=="Europe/Amsterdam" || z=="UTC" || z=="Europe/London" ||
         z=="America/New_York" || z=="Australia/Sydney";
}
inline const char* posixZone(const char* z) {
  if(strcmp(z,"Europe/Amsterdam")==0)return "CET-1CEST,M3.5.0/2,M10.5.0/3";
  if(strcmp(z,"Europe/London")==0)return "GMT0BST,M3.5.0/1,M10.5.0/2";
  if(strcmp(z,"America/New_York")==0)return "EST5EDT,M3.2.0/2,M11.1.0/2";
  if(strcmp(z,"Australia/Sydney")==0)return "AEST-10AEDT,M10.1.0/2,M4.1.0/3";
  return "UTC0";
}
inline bool validSSID(const String& s) {return s.length()<=32 && safeText(s,33);}
inline bool validHost(const String& s) {
  if(s.isEmpty())return true;  // hardware-based default hostname
  if(s.length()>63 || s[0]=='-' || s[s.length()-1]=='-')return false;
  for(size_t i=0;i<s.length();i++){
    char c=s[i];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
         (c>='0'&&c<='9')||c=='-'))return false;
  }
  return true;
}
inline bool validAP(const String& password) {
  return password.length()==0 || (password.length()>=8 && password.length()<=63
                                  && safeText(password,65));
}
struct Settings {
  uint32_t magic=0x47574335;
  uint32_t version=1;
  char wifiSSID[33]={};
  char wifiPassword[65]={};
  char hostname[64]={};
  char mqttHost[129]={};
  uint16_t mqttPort=1883;
  char mqttUser[65]={};
  char mqttPassword[65]={};
  char timezone[32]={};
  char recoveryPassword[65]={};
  char webUser[33]={};
  char webPassword[65]={};
  uint32_t rfFrequencyKhz=868300;
  uint32_t otBridge=0;
  uint32_t otSource=0;
  uint8_t otScanAll=0;
  uint8_t otAutoPoll=0;
  uint8_t otTxEnabled=0;
  uint8_t reserved=0;
  uint32_t checksum=0;
};
static_assert(sizeof(Settings)<1024,"NVS configuration must stay bounded");
inline uint32_t crc(const Settings& s) {
  const uint8_t* ptr=reinterpret_cast<const uint8_t*>(&s);
  uint32_t hash=2166136261u;
  for(size_t i=0;i<offsetof(Settings,checksum);i++)
    hash=(hash^ptr[i])*16777619u;
  return hash;
}
inline void copy(char* dest,size_t capacity,const char* source){
  snprintf(dest,capacity,"%s",source?source:"");
}
inline bool validate(const Settings& s, uint16_t hardwareBand) {
  if(s.magic!=0x47574335 || s.version!=1) return false;
  if(!validSSID(s.wifiSSID)||!safeText(s.wifiPassword,sizeof(s.wifiPassword))||
     !validHost(s.hostname)||!safeText(s.mqttHost,sizeof(s.mqttHost))||
     !safeText(s.mqttUser,sizeof(s.mqttUser))||
     !safeText(s.mqttPassword,sizeof(s.mqttPassword))||
     !validTimeZone(s.timezone) || !validAP(s.recoveryPassword) ||
     !safeText(s.webUser,sizeof(s.webUser))||
     !safeText(s.webPassword,sizeof(s.webPassword)) ||
     s.mqttPort==0) return false;
  if(!radio_frequency::withinHardwareBand(s.rfFrequencyKhz,hardwareBand))
    return false;
  if(s.otBridge>262143 || s.otSource>262142 || s.otSource==730 ||
     s.otScanAll>1 || s.otAutoPoll>1 || s.otTxEnabled>1)
    return false;
  // RF transmit cannot be armed without a valid bridge & gateway identity.
  if(s.otTxEnabled && (!s.otBridge || !s.otSource ||
      !radio_frequency::evohomeTxFrequency(s.rfFrequencyKhz,hardwareBand)))
    return false;
  if(s.webPassword[0] && !s.webUser[0])return false;
  return true;
}
inline bool sealed(const Settings& s,uint16_t band) {
  return crc(s)==s.checksum && validate(s,band);
}
inline void seal(Settings& s){s.checksum=crc(s);}
inline Settings defaults() {
  Settings s;
  copy(s.wifiSSID,sizeof(s.wifiSSID),GW_WIFI_SSID);
  copy(s.wifiPassword,sizeof(s.wifiPassword),GW_WIFI_PASSWORD);
  copy(s.hostname,sizeof(s.hostname),GW_HOSTNAME);
  copy(s.mqttHost,sizeof(s.mqttHost),GW_MQTT_HOST);
  s.mqttPort=GW_MQTT_PORT;
  copy(s.mqttUser,sizeof(s.mqttUser),GW_MQTT_USER);
  copy(s.mqttPassword,sizeof(s.mqttPassword),GW_MQTT_PASSWORD);
  copy(s.timezone,sizeof(s.timezone),GW_TIMEZONE);
  copy(s.recoveryPassword,sizeof(s.recoveryPassword),GW_RECOVERY_AP_PASSWORD);
  copy(s.webUser,sizeof(s.webUser),GW_DEBUG_USER);
  copy(s.webPassword,sizeof(s.webPassword),GW_DEBUG_PASSWORD);
  s.rfFrequencyKhz=GW_CC1101_FREQUENCY_KHZ;
  s.otBridge=GW_OT_BRIDGE_ID;
  s.otSource=GW_OT_SOURCE_ID;
  s.otScanAll=GW_OT_SCAN_ALL_256?1:0;
  s.otAutoPoll=GW_OT_AUTO_POLL?1:0;
  // Keep RF transmit OFF on every fresh installation, regardless of defaults.
  s.otTxEnabled=0;
  seal(s);
  return s;
}
inline bool loadRecord(Preferences& p,const char* key,Settings& dst,
                       uint16_t band) {
  Settings candidate;
  if(p.getBytesLength(key)!=sizeof(candidate) ||
     p.getBytes(key,&candidate,sizeof(candidate))!=sizeof(candidate) ||
     !sealed(candidate,band))return false;
  dst=candidate;return true;
}
class Store {
 public:
  Settings current={};
  bool pending=false;
  void begin(uint16_t moduleBand){
    current=defaults();
    Preferences p;
    if(!p.begin("gw-settings",true))return;
    Settings saved;
    if(loadRecord(p,"config",saved,moduleBand))current=saved;
    pending=p.getBool("pending",false);
    p.end();
    if(!validate(current,moduleBand)){
      current=defaults();pending=false;
    }
  }
  bool stage(const Settings& candidate,uint16_t band){
    if(!validate(candidate,band))return false;
    Settings toSave=candidate;seal(toSave);
    Preferences p;
    if(!p.begin("gw-settings",false))return false;
    bool ok=p.putBytes("previous",&current,sizeof(current))==sizeof(current);
    // Write pending BEFORE the candidate config. If power fails between
    // writes, we recover from previous instead of trusting new credentials.
    if(ok)ok=p.putBool("pending",true);
    if(ok)ok=p.putBytes("config",&toSave,sizeof(toSave))==sizeof(toSave);
    p.end();
    if(ok)pending=true;
    return ok;
  }
  bool confirm(){
    Preferences p;
    if(!p.begin("gw-settings",false))return false;
    const bool ok=p.putBool("pending",false);
    p.end();if(ok)pending=false;
    return ok;
  }
  bool rollback(uint16_t band) {
    Preferences p;
    if(!p.begin("gw-settings",false))return false;
    Settings previous;
    bool ok=loadRecord(p,"previous",previous,band);
    if(ok)ok=p.putBytes("config",&previous,sizeof(previous))==sizeof(previous);
    if(ok)ok=p.putBool("pending",false);
    p.end();return ok;
  }
};
}  // namespace gw_config
