#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdio>
#define GW_WIFI_SSID "my-home"
#define GW_WIFI_PASSWORD "not-a-real-password"
#define GW_HOSTNAME "evohome-gateway"
#define GW_MQTT_HOST "192.0.2.8"
#define GW_MQTT_PORT 1883
#define GW_MQTT_DISCOVERY 1
#define GW_MQTT_USER "broker"
#define GW_MQTT_PASSWORD "not-a-real-password"
#define GW_TIMEZONE "Europe/Amsterdam"
#define GW_RECOVERY_AP_PASSWORD "test-recovery-password"
#define GW_DEBUG_USER "admin"
#define GW_DEBUG_PASSWORD "test-diagnostic-password"
#define GW_CC1101_FREQUENCY_KHZ 868300
#define GW_CC1101_MODULE_BAND 868
#define GW_OT_BRIDGE_ID 0
#define GW_OT_SOURCE_ID 0
#define GW_OT_SCAN_ALL_256 0
#define GW_OT_AUTO_POLL 0
#include "../RamsesGateway/GatewayRuntimeConfig.h"

int main(){
  using namespace gw_config;
  Settings defaultsConfig=defaults();
  assert(sealed(defaultsConfig,868));
  assert(strcmp(defaultsConfig.timezone,"Europe/Amsterdam")==0);
  assert(defaultsConfig.otTxEnabled==0);
  assert(defaultsConfig.version==2 && defaultsConfig.rfModuleBand==868);
  assert(defaultsConfig.rfExperimentalRx==0);
  assert(defaultsConfig.mqttDiscovery==1); // RF disabled on first boot.
  assert(std::strcmp(posixZone("Europe/Amsterdam"),
         "CET-1CEST,M3.5.0/2,M10.5.0/3")==0);
  assert(validTimeZone("Europe/Amsterdam") && !validTimeZone("Invalid/Zone"));
  assert(!validHost("-hostname") && !validHost("a b") && validHost("ramses-home"));
  assert(!validAP("short") && validAP("strong-enough"));
  assert(!radio_frequency::withinHardwareBand(433920,868)); // see namespace below
  Settings cross=defaultsConfig;
  cross.rfModuleBand=433;
  assert(!validate(cross,868)); // no silent cross-band RX
  cross.rfExperimentalRx=1;
  assert(validate(cross,868)); // explicitly opted-in RX
  cross.otBridge=125802;cross.otSource=123456;cross.otTxEnabled=1;
  assert(!validate(cross,868)); // cross-band mode cannot transmit
  cross.otTxEnabled=0;
  assert(validate(cross,868));
  assert(!validate(Settings{},0));
  Settings chosen=defaultsConfig;
  chosen.rfFrequencyKhz=433920;
  seal(chosen);
  assert(!sealed(chosen,868));
  chosen.rfFrequencyKhz=868300;
  chosen.otTxEnabled=1; // must have both IDs + Evohome frequency
  assert(!validate(chosen,868));
  chosen.otBridge=125802;chosen.otSource=123456;
  assert(validate(chosen,868));
  chosen.timezone[0]='X';
  assert(!validate(chosen,868));
  chosen=defaultsConfig;
  chosen.mqttDiscovery=2;
  assert(!validate(chosen,868));
  chosen=defaultsConfig;
  Store store;store.begin(868);
  assert(!store.pending);
  Preferences::resetWriteCount();
  assert(store.unchanged(store.current,868));
  assert(store.stage(store.current,868));
  assert(!store.pending && Preferences::writeCount()==0);
  Settings updated=store.current;
  updated.rfModuleBand=433;
  updated.rfExperimentalRx=1;
  copy(updated.hostname,sizeof(updated.hostname),"test-gateway");
  copy(updated.timezone,sizeof(updated.timezone),"UTC");
  updated.mqttPort=1884;
  updated.mqttDiscovery=0;
  assert(!store.unchanged(updated,868));
  assert(store.stage(updated,868) && store.pending);
  assert(Preferences::writeCount()==3); // previous, pending, candidate
  assert(!store.unchanged(updated,868)); // pending never bypasses recovery
  Store pending;pending.begin(868);
  assert(pending.pending && strcmp(pending.current.hostname,"test-gateway")==0);
  assert(pending.current.rfModuleBand==433 && pending.current.rfExperimentalRx==1);
  assert(strcmp(pending.current.timezone,"UTC")==0);
  assert(pending.rollback(868));
  Store previous;previous.begin(868);
  assert(!previous.pending && strcmp(previous.current.hostname,"evohome-gateway")==0);
  assert(store.stage(updated,868));
  Store committed;committed.begin(868);
  assert(committed.pending && committed.confirm());
  Store persisted;persisted.begin(868);
  assert(!persisted.pending && persisted.current.rfModuleBand==433 &&
         persisted.current.rfExperimentalRx==1);
  assert(!persisted.pending && persisted.current.mqttPort==1884 &&
         persisted.current.mqttDiscovery==0);
  Preferences::resetWriteCount();
  assert(persisted.unchanged(persisted.current,868));
  assert(persisted.stage(persisted.current,868) && !persisted.pending);
  assert(Preferences::writeCount()==0);
  // Upgrade a genuine v1 binary record, preserving network credentials.
  LegacySettingsV1 old={};
  gw_config::copy(old.wifiSSID,sizeof(old.wifiSSID),defaultsConfig.wifiSSID);
  gw_config::copy(old.wifiPassword,sizeof(old.wifiPassword),defaultsConfig.wifiPassword);
  gw_config::copy(old.hostname,sizeof(old.hostname),defaultsConfig.hostname);
  gw_config::copy(old.webUser,sizeof(old.webUser),defaultsConfig.webUser);
  gw_config::copy(old.webPassword,sizeof(old.webPassword),defaultsConfig.webPassword);
  gw_config::copy(old.timezone,sizeof(old.timezone),defaultsConfig.timezone);
  old.mqttPort=defaultsConfig.mqttPort;
  old.rfFrequencyKhz=defaultsConfig.rfFrequencyKhz;
  old.mqttDiscovery=defaultsConfig.mqttDiscovery;
  old.version=1;
  old.checksum=legacyCrc(old);
  Preferences prefs;
  assert(prefs.begin("gw-settings",false));
  assert(prefs.putBytes("config",&old,sizeof(old))==sizeof(old));
  prefs.end();
  Store migrated;migrated.begin(868);
  assert(migrated.current.version==2);
  assert(strcmp(migrated.current.wifiSSID,"my-home")==0);
  assert(strcmp(migrated.current.webPassword,"test-diagnostic-password")==0);
  assert(migrated.current.rfModuleBand==868 && migrated.current.rfExperimentalRx==0);
  // Verify POSIX DST rules actually change local time.
  const char* zone=posixZone("Europe/Amsterdam");
  setenv("TZ",zone,1);tzset();
  std::tm january={}; january.tm_year=126;january.tm_mon=0;january.tm_mday=1;
  std::tm july={}; july.tm_year=126;july.tm_mon=6;july.tm_mday=1;
  const time_t janUtc=timegm(&january),julUtc=timegm(&july);
  std::tm jan={},jul={};localtime_r(&janUtc,&jan);localtime_r(&julUtc,&jul);
  assert(jan.tm_hour==1 && jul.tm_hour==2);
  assert(jan.tm_isdst==0 && jul.tm_isdst==1);
  std::puts("NVS config defaults, validation, staging, rollback, CET/CEST DST PASS");
}
