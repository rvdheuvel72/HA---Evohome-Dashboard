#include <cassert>
#include <cstdio>
#include "../RamsesGateway/RamsesRxBypass.h"
int main() {
  using namespace ramses_rx_bypass;
  Burst b;
  constexpr uint32_t freq=0x21656A;
  assert(!b.config_ok(freq));
  assert(!b.uniform());
  b.sampled=true;
  for(auto& x:b.regs)x=0x1f;
  assert(b.uniform());
  assert(!b.config_ok(freq));
  b.regs[0]=ramses_spi_audit::IOCFG2_RX;
  b.regs[8]=ramses_spi_audit::PKTCTRL0_RX;
  b.regs[18]=ramses_spi_audit::MDMCFG2_RAMSES;
  b.regs[13]=0x21;
  b.regs[14]=0x65;
  b.regs[15]=0x6A;
  assert(b.config_ok(freq));
  assert(!b.uniform());
  assert(!b.config_ok(freq+1));
  b.regs[8]=0x1f;
  assert(!b.config_ok(freq));
  ramses_spi_audit::Snapshot bad{};
  assert(!shouldTry(bad,false));  // no snapshot is not proof
  bad.sampled=true;
  bad.part=bad.version=bad.marc_raw=0x1f;
  bad.iocfg2_a=bad.iocfg2_b=bad.pktctrl0_a=bad.pktctrl0_b=0x1f;
  bad.mdmcfg2=bad.freq2=bad.freq1=bad.freq0=0x1f;
  bad.expected_freq_word=freq;
  assert(shouldTry(bad,false));
  assert(!shouldTry(bad,true));  // previously restored by safe slow SPI
  bad.part=0; bad.version=0x14;
  bad.iocfg2_a=bad.iocfg2_b=0x0d;
  bad.pktctrl0_a=bad.pktctrl0_b=0x32;
  bad.mdmcfg2=0x10;
  bad.freq2=0x21;bad.freq1=0x65;bad.freq0=0x6a;
  assert(bad.config_ok());
  assert(!shouldTry(bad,false));
  Result result{};
  assert(!result.attempted);
  assert(!result.reentry_issued);
  assert(restoreIssued(result)); // no SIDLE, so RX not intentionally interrupted
  result.attempted=true;
  assert(restoreIssued(result));
  result.sidle_issued=true;
  assert(!restoreIssued(result)); // P2: do not report ready with radio in IDLE
  result.reentry_issued=true;
  assert(restoreIssued(result));
  result.reentry_issued=false;
  assert(!restoreIssued(result));
  result.fallback_srx_issued=true;
  assert(restoreIssued(result)); // a single bounded SRX-only fallback
  puts("CC1101 RX bypass policy and continuous-burst tests PASS");
}
