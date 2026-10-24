// Pure CC1101 RX recovery validation. No hardware writes or radio actions.
// In particular, normal RF silence is never evidence of failed SPI.
#pragma once
#include <stdint.h>
#include "RamsesSpiAudit.h"
namespace ramses_rx_bypass {
struct Burst {
  uint32_t uptime_ms=0;
  uint8_t chip_status=0xFF;
  uint8_t regs[19]={}; // IOCFG2 .. MDMCFG2 (CC1101 normal registers 0x00-0x12)
  bool sampled=false;
  bool config_ok(uint32_t expected_freq) const {
    return sampled && regs[0]==ramses_spi_audit::IOCFG2_RX &&
      regs[8]==ramses_spi_audit::PKTCTRL0_RX &&
      regs[18]==ramses_spi_audit::MDMCFG2_RAMSES &&
      ((static_cast<uint32_t>(regs[13])<<16) |
       (static_cast<uint32_t>(regs[14])<<8) | regs[15])==expected_freq;
  }
  bool uniform() const {
    if(!sampled)return false;
    for(unsigned i=1;i<19;i++)if(regs[i]!=regs[0])return false;
    return true;
  }
};
struct Result {
  bool attempted=false;
  bool sidle_issued=false;
  bool reentry_issued=false;
  bool fallback_srx_issued=false;
  bool restored_valid_spi=false;
  Burst rx_burst{};
  Burst idle_burst{};
  ramses_spi_audit::Snapshot idle_audit{};
  ramses_spi_audit::Snapshot resumed_audit{};
};
// An issued SIDLE may have removed RX. We MUST not report initialization
// success unless a subsequent restore command (full path or SRX fallback)
// was successfully issued, even if chip register readbacks are corrupt.
inline bool restoreIssued(const Result& r) {
  return !r.attempted || !r.sidle_issued ||
    r.reentry_issued || r.fallback_srx_issued;
}
inline bool shouldTry(const ramses_spi_audit::Snapshot& afterRx,
                      bool slowSpiAlreadyRestored) {
  return afterRx.sampled && !afterRx.config_ok() && !slowSpiAlreadyRestored;
}
} // namespace ramses_rx_bypass
