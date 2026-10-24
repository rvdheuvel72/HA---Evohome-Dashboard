#include <cassert>
#include <cstdio>
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
  puts("Read-only CC1101 SPI audit tests PASS");
}
