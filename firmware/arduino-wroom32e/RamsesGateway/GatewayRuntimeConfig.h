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
#ifndef GW_CC1101_MODULE_BAND
#define GW_CC1101_MODULE_BAND 868 // Matches the default in RamsesGateway.ino
#endif

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
struct LegacySettingsV1 {
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
  uint8_t mqttDiscovery=1;
  uint32_t checksum=0;
};
struct Settings {
  uint32_t magic=0x47574335;
  uint32_t version=2;
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
  uint8_t mqttDiscovery=1;
  // User-declared physical PCB marking; cannot be verified by software.
  uint16_t rfModuleBand=868;
  uint8_t rfExperimentalRx=0;
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
  if(s.magic!=0x47574335 || s.version!=2) return false;
  if(!validSSID(s.wifiSSID)||!safeText(s.wifiPassword,sizeof(s.wifiPassword))||
     !validHost(s.hostname)||!safeText(s.mqttHost,sizeof(s.mqttHost))||
     !safeText(s.mqttUser,sizeof(s.mqttUser))||
     !safeText(s.mqttPassword,sizeof(s.mqttPassword))||
     !validTimeZone(s.timezone) || !validAP(s.recoveryPassword) ||
     !safeText(s.webUser,sizeof(s.webUser))||
     !safeText(s.webPassword,sizeof(s.webPassword)) ||
     s.mqttPort==0) return false;
  if(!radio_frequency::withinHardwareBand(s.rfFrequencyKhz,s.rfModuleBand) &&
     !(s.rfExperimentalRx && radio_frequency::withinChipBand(s.rfFrequencyKhz)))
    return false;
  if((s.rfModuleBand!=315 && s.rfModuleBand!=433 &&
      s.rfModuleBand!=868 && s.rfModuleBand!=915) || s.rfExperimentalRx>1 ||
      (hardwareBand!=315 && hardwareBand!=433 && hardwareBand!=868 && hardwareBand!=915))
    return false;
  if(s.otBridge>262143 || s.otSource>262142 || s.otSource==730 ||
     s.otScanAll>1 || s.otAutoPoll>1 || s.otTxEnabled>1 || s.mqttDiscovery>1)
    return false;
  // RF transmit cannot be armed without a valid bridge & gateway identity.
  if(s.otTxEnabled && (!s.otBridge || !s.otSource ||
      !radio_frequency::evohomeTxFrequency(s.rfFrequencyKhz,s.rfModuleBand) ||
       s.rfExperimentalRx))
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
  s.rfModuleBand=GW_CC1101_MODULE_BAND;
  s.rfExperimentalRx=0;
  s.otBridge=GW_OT_BRIDGE_ID;
  s.otSource=GW_OT_SOURCE_ID;
  s.otScanAll=GW_OT_SCAN_ALL_256?1:0;
  s.otAutoPoll=GW_OT_AUTO_POLL?1:0;
  s.mqttDiscovery=GW_MQTT_DISCOVERY?1:0;
  // Keep RF transmit OFF on every fresh installation, regardless of defaults.
  s.otTxEnabled=0;
  seal(s);
  return s;
}
// Preserve Wi-Fi and HTTP credentials when upgrading existing v1 NVS records.
inline uint32_t legacyCrc(const LegacySettingsV1& s){
  const uint8_t* ptr=reinterpret_cast<const uint8_t*>(&s);
  uint32_t hash=2166136261u;
  for(size_t i=0;i<offsetof(LegacySettingsV1,checksum);++i)
    hash=(hash^ptr[i])*16777619u;
  return hash;
}
inline bool loadLegacyRecord(Preferences& p,const char* key,Settings& dst,
                             uint16_t defaultBand){
  LegacySettingsV1 legacy;
  if(p.getBytesLength(key)!=sizeof(legacy) ||
     p.getBytes(key,&legacy,sizeof(legacy))!=sizeof(legacy) ||
     legacy.magic!=0x47574335 || legacy.version!=1 ||
     legacy.checksum!=legacyCrc(legacy))return false;
  Settings migrated=defaults();
  // Copy named fields instead of object memory: safe across layout changes.
  copy(migrated.wifiSSID,sizeof(migrated.wifiSSID),legacy.wifiSSID);
  copy(migrated.wifiPassword,sizeof(migrated.wifiPassword),legacy.wifiPassword);
  copy(migrated.hostname,sizeof(migrated.hostname),legacy.hostname);
  copy(migrated.mqttHost,sizeof(migrated.mqttHost),legacy.mqttHost);
  migrated.mqttPort=legacy.mqttPort;
  copy(migrated.mqttUser,sizeof(migrated.mqttUser),legacy.mqttUser);
  copy(migrated.mqttPassword,sizeof(migrated.mqttPassword),legacy.mqttPassword);
  copy(migrated.timezone,sizeof(migrated.timezone),legacy.timezone);
  copy(migrated.recoveryPassword,sizeof(migrated.recoveryPassword),legacy.recoveryPassword);
  copy(migrated.webUser,sizeof(migrated.webUser),legacy.webUser);
  copy(migrated.webPassword,sizeof(migrated.webPassword),legacy.webPassword);
  migrated.rfFrequencyKhz=legacy.rfFrequencyKhz;
  migrated.otBridge=legacy.otBridge;
  migrated.otSource=legacy.otSource;
  migrated.otScanAll=legacy.otScanAll;
  migrated.otAutoPoll=legacy.otAutoPoll;
  migrated.otTxEnabled=legacy.otTxEnabled;
  migrated.mqttDiscovery=legacy.mqttDiscovery;
  migrated.version=2;
  migrated.rfModuleBand=defaultBand;
  migrated.rfExperimentalRx=0;
  seal(migrated);
  if(!sealed(migrated,defaultBand))return false;
  dst=migrated;
  return true;
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
    if(loadRecord(p,"config",saved,moduleBand) ||
       loadLegacyRecord(p,"config",saved,moduleBand))current=saved;
    pending=p.getBool("pending",false);
    p.end();
    if(!validate(current,moduleBand)){
      current=defaults();pending=false;
    }
  }
  // A byte-identical, already confirmed configuration needs no new NVS
  // transaction. Pending updates must still run the full rollback protocol.
  bool unchanged(const Settings& candidate,uint16_t band) const {
    if(pending || !validate(candidate,band))return false;
    Settings sealedCandidate=candidate;
    seal(sealedCandidate);
    return memcmp(&current,&sealedCandidate,sizeof(Settings))==0;
  }
  bool stage(const Settings& candidate,uint16_t band){
    if(!validate(candidate,band))return false;
    if(unchanged(candidate,band))return true;
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
    bool ok=loadRecord(p,"previous",previous,band) ||
            loadLegacyRecord(p,"previous",previous,band);
    if(ok)ok=p.putBytes("config",&previous,sizeof(previous))==sizeof(previous);
    if(ok)ok=p.putBool("pending",false);
    p.end();return ok;
  }
};
}  // namespace gw_config
