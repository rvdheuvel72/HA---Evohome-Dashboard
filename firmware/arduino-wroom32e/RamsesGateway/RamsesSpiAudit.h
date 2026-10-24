// Read-only CC1101 SPI register diagnostics, independent of Arduino runtime.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
namespace ramses_spi_audit {
constexpr uint8_t IOCFG2_RX = 0x0D;
constexpr uint8_t PKTCTRL0_RX = 0x32;
constexpr uint8_t MDMCFG2_RAMSES = 0x10;
constexpr uint8_t PARTNUM_CC1101 = 0x00;
constexpr uint8_t VERSION_CC1101 = 0x14;
struct Snapshot {
  uint32_t uptime_ms=0;
  uint8_t part=0xFF,version=0xFF;
  uint8_t marc_raw=0xFF;
  uint8_t iocfg2_a=0xFF,iocfg2_b=0xFF;
  uint8_t pktctrl0_a=0xFF,pktctrl0_b=0xFF;
  uint8_t mdmcfg2=0xFF;
  uint8_t freq2=0xFF,freq1=0xFF,freq0=0xFF;
  uint32_t expected_freq_word=0;
  bool sampled=false;
  uint32_t received_freq_word() const {
    return (static_cast<uint32_t>(freq2)<<16) |
           (static_cast<uint32_t>(freq1)<<8) | freq0;
  }
  // An equal value in every register, including ones programmed with different
  // data, means SPI readback cannot currently be trusted. It is not proof of
  // which physical wire is defective.
  bool uniform_data() const {
    return sampled && iocfg2_a==iocfg2_b && iocfg2_a==pktctrl0_a &&
           iocfg2_a==pktctrl0_b && iocfg2_a==mdmcfg2 &&
           iocfg2_a==freq2 && iocfg2_a==freq1 && iocfg2_a==freq0;
  }
  bool identifiers_ok() const {
    return sampled && part==PARTNUM_CC1101 &&
           version==VERSION_CC1101;
  }
  bool config_ok() const {
    return sampled && identifiers_ok() && !uniform_data() &&
           iocfg2_a==IOCFG2_RX && iocfg2_b==IOCFG2_RX &&
           pktctrl0_a==PKTCTRL0_RX && pktctrl0_b==PKTCTRL0_RX &&
           mdmcfg2==MDMCFG2_RAMSES &&
           received_freq_word()==expected_freq_word;
  }
  const char* diagnosis() const {
    if(!sampled) return "not_sampled";
    if(uniform_data())return "uniform_spi_readback_suspected";
    if(!identifiers_ok())return "chip_identity_unreliable";
    if(!config_ok())return "register_configuration_mismatch";
    // Even healthy SPI does not prove working RF reception.
    return "config_readback_matches";
  }
};
// Compact format sized for GatewayDebug::LINE_BYTES (144 bytes). Captures
// original register bytes, not the 5-bit interpretation of MARCSTATE.
// A truncated line is never sent to the log because it could be misleading.
inline bool formatStartupLine(const char* phase,const Snapshot& s,
                              char* out,size_t capacity) {
  if(!phase || !out || !capacity || !s.sampled)return false;
  const int n=snprintf(out,capacity,
    "SPI-%s t=%lu part=%02X ver=%02X marc=%02X io2=%02X/%02X pkt=%02X/%02X mdm=%02X freq=%02X%02X%02X expected=%06lX uni=%u",
    phase,static_cast<unsigned long>(s.uptime_ms),
    static_cast<unsigned>(s.part),static_cast<unsigned>(s.version),
    static_cast<unsigned>(s.marc_raw),
    static_cast<unsigned>(s.iocfg2_a),static_cast<unsigned>(s.iocfg2_b),
    static_cast<unsigned>(s.pktctrl0_a),static_cast<unsigned>(s.pktctrl0_b),
    static_cast<unsigned>(s.mdmcfg2),
    static_cast<unsigned>(s.freq2),static_cast<unsigned>(s.freq1),
    static_cast<unsigned>(s.freq0),
    static_cast<unsigned long>(s.expected_freq_word),
    s.uniform_data()?1u:0u);
  return n>0 && static_cast<size_t>(n)<capacity;
}
} // namespace ramses_spi_audit
