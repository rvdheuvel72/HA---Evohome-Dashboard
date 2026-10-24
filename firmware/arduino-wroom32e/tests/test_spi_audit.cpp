#include <cassert>
#include <cstdio>
#include <cstring>
#include "../RamsesGateway/RamsesSpiAudit.h"
int main() {
  using namespace ramses_spi_audit;
  Snapshot a;
  assert(!a.sampled && !a.config_ok());
  assert(a.diagnosis()[0]=='n');
  a.sampled=true;
  a.uptime_ms=1200;
  a.part=0x00;
  a.version=0x14;
  a.marc_raw=0x1f;
  a.iocfg2_a=a.iocfg2_b=0x1f;
  a.pktctrl0_a=a.pktctrl0_b=0x1f;
  a.mdmcfg2=0x1f;
  a.freq2=a.freq1=a.freq0=0x1f;
  a.expected_freq_word=0x21656A;
  assert(a.identifiers_ok());
  assert(a.uniform_data());
  assert(!a.config_ok());
  assert(a.received_freq_word()==0x1f1f1f);
  assert(a.diagnosis()[0]=='u');
  // Known-good serial configuration and RF synth register readbacks.
  a.marc_raw=0x0d;
  a.iocfg2_a=a.iocfg2_b=IOCFG2_RX;
  a.pktctrl0_a=a.pktctrl0_b=PKTCTRL0_RX;
  a.mdmcfg2=MDMCFG2_RAMSES;
  a.freq2=0x21; a.freq1=0x65; a.freq0=0x6a;
  assert(!a.uniform_data());
  assert(a.config_ok());
  assert(a.diagnosis()[0]=='c');
  // Identifiers alone don't prove radio configured or able to RX.
  a.pktctrl0_b=0x1f;
  assert(!a.config_ok());
  assert(a.diagnosis()[0]=='r');
  a.pktctrl0_b=PKTCTRL0_RX;
  a.part=0xff;
  assert(!a.identifiers_ok() && !a.config_ok());
  assert(a.diagnosis()[0]=='c'); // chip_identity_unreliable
  // Only the timestamp may change without producing a new runtime log.
  Snapshot reference=a;
  Snapshot update=reference;
  update.uptime_ms+=30000;
  assert(!update.changedFrom(reference));
  update.mdmcfg2^=1u;
  assert(update.changedFrom(reference)); // modem read corruption
  update=reference;update.iocfg2_b^=1u;
  assert(update.changedFrom(reference)); // duplicate read disagreement
  update=reference;update.pktctrl0_b^=1u;
  assert(update.changedFrom(reference));
  update=reference;update.freq0^=1u;
  assert(update.changedFrom(reference));
  update=reference;update.marc_raw^=1u;
  assert(update.changedFrom(reference));
  update=reference;update.part^=1u;
  assert(update.changedFrom(reference));
  update=reference;update.part_header^=1u;
  assert(update.changedFrom(reference)); // SPI first header response
  update=reference;update.marc_header^=1u;
  assert(update.changedFrom(reference)); // MARC first byte
  update=reference;update.io2_header^=1u;
  assert(update.changedFrom(reference)); // config first byte
  // Boot timeline must preserve all raw bytes in a single 144-byte
  // GatewayDebug entry and reject any potentially truncated result.
  a.part=0;
  a.version=0x14;
  a.marc_raw=0x1f;
  a.iocfg2_a=a.iocfg2_b=0x1f;
  a.pktctrl0_a=a.pktctrl0_b=0x1f;
  a.mdmcfg2=0x1f;
  a.freq2=a.freq1=a.freq0=0x1f;
  char line[144]={};
  assert(formatStartupLine("pre",a,line,sizeof(line)));
  assert(strstr(line,"SPI-pre") && strstr(line,"part=00") && strstr(line,"ver=14"));
  assert(strstr(line,"marc=1F") && strstr(line,"io2=1F/1F"));
  assert(strstr(line,"pkt=1F/1F") && strstr(line,"freq=1F1F1F"));
  assert(strstr(line,"uni=1"));
  assert(strstr(line,"hdr=FF/FF/FF")); // before header probes run
  a.part_header=0x1F;
  a.marc_header=0x1F;
  a.io2_header=0x1F;
  assert(formatStartupLine("srx-now",a,line,sizeof(line)));
  assert(strstr(line,"hdr=1F/1F/1F"));
  assert(strstr(line,"SPI-srx-now"));
  assert(formatStartupLine("packet",a,line,sizeof(line)));
  char tiny[32]={};
  assert(!formatStartupLine("pre",a,tiny,sizeof(tiny)));
  a.sampled=false;
  assert(!formatStartupLine("pre",a,line,sizeof(line)));
  // Regression: M5Stack beta.23 returned 0x1F from EVERY register. The
  // extra chip-status header fields must not overflow the HTTP response.
  Snapshot bad{};
  bad.sampled=true;bad.uptime_ms=30000;
  bad.expected_freq_word=0x21656A;
  bad.part=bad.version=bad.marc_raw=0x1F;
  bad.part_header=bad.marc_header=bad.io2_header=0x1F;
  bad.iocfg2_a=bad.iocfg2_b=0x1F;
  bad.pktctrl0_a=bad.pktctrl0_b=0x1F;
  bad.mdmcfg2=bad.freq2=bad.freq1=bad.freq0=0x1F;
  char json[768]={};
  assert(formatAuditJson(bad,3,json,sizeof(json)));
  assert(strstr(json,"\"uniform_spi_readback_suspected\""));
  assert(strstr(json,"\"chip_status_at_part_header\":31"));
  assert(strstr(json,"\"chip_status_at_marc_header\":31"));
  assert(strstr(json,"\"chip_status_at_io2_header\":31"));
  assert(strstr(json,"\"expected_freq_word\":2188650"));
  assert(strstr(json,"\"read_only\":true"));
  assert(strstr(json,"\"freq0\":31"));
  assert(json[strlen(json)-1]=='}');
  char tooSmall[550]={};
  assert(!formatAuditJson(bad,3,tooSmall,sizeof(tooSmall)));
  assert(!formatAuditJson(bad,3,nullptr,0));
  // Largest textual uint32 values also fit the bounded response.
  bad.uptime_ms=0xFFFFFFFFu;bad.expected_freq_word=0xFFFFFFFFu;
  assert(formatAuditJson(bad,0xFFFFFFFFu,json,sizeof(json)));
  puts("Read-only CC1101 SPI audit tests PASS");
}
