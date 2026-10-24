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
  // CC1101 returns a chip status byte during the SPI header transfer,
  // then the requested data byte. Keep these values separate.
  uint8_t part_header=0xFF,marc_header=0xFF,io2_header=0xFF;
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
  // Ignore uptime: it changes on every sample. Compare every SPI value,
  // including duplicate reads, so intermittent corruption is never hidden.
  bool changedFrom(const Snapshot& previous) const {
    return sampled!=previous.sampled || part!=previous.part ||
      version!=previous.version || marc_raw!=previous.marc_raw ||
      part_header!=previous.part_header ||
      marc_header!=previous.marc_header ||
      io2_header!=previous.io2_header ||
      iocfg2_a!=previous.iocfg2_a || iocfg2_b!=previous.iocfg2_b ||
      pktctrl0_a!=previous.pktctrl0_a || pktctrl0_b!=previous.pktctrl0_b ||
      mdmcfg2!=previous.mdmcfg2 ||
      freq2!=previous.freq2 || freq1!=previous.freq1 ||
      freq0!=previous.freq0 || expected_freq_word!=previous.expected_freq_word;
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
// Read-only clock sweep. A failed normal-rate register read is NOT proof
// that the module is defective. A slower SPI rate is retained only when
// two consecutive independent readbacks verify the entire configuration.
inline bool slowProbeNeeded(const Snapshot& normal) {
  return normal.sampled && !normal.config_ok();
}
inline bool keepSlowClock(const Snapshot& first,const Snapshot& second) {
  return first.config_ok() && second.config_ok() &&
    !first.changedFrom(second);
}
// Compact format sized for GatewayDebug::LINE_BYTES (144 bytes). Captures
// original register bytes, not the 5-bit interpretation of MARCSTATE.
// A truncated line is never sent to the log because it could be misleading.
inline bool formatStartupLine(const char* phase,const Snapshot& s,
                              char* out,size_t capacity) {
  if(!phase || !out || !capacity || !s.sampled)return false;
  const int n=snprintf(out,capacity,
    "SPI-%s t=%lu part=%02X ver=%02X marc=%02X io2=%02X/%02X pkt=%02X/%02X mdm=%02X freq=%02X%02X%02X expected=%06lX uni=%u hdr=%02X/%02X/%02X",
    phase,static_cast<unsigned long>(s.uptime_ms),
    static_cast<unsigned>(s.part),static_cast<unsigned>(s.version),
    static_cast<unsigned>(s.marc_raw),
    static_cast<unsigned>(s.iocfg2_a),static_cast<unsigned>(s.iocfg2_b),
    static_cast<unsigned>(s.pktctrl0_a),static_cast<unsigned>(s.pktctrl0_b),
    static_cast<unsigned>(s.mdmcfg2),
    static_cast<unsigned>(s.freq2),static_cast<unsigned>(s.freq1),
    static_cast<unsigned>(s.freq0),
    static_cast<unsigned long>(s.expected_freq_word),
    s.uniform_data()?1u:0u,
    static_cast<unsigned>(s.part_header),
    static_cast<unsigned>(s.marc_header),
    static_cast<unsigned>(s.io2_header));
  return n>0 && static_cast<size_t>(n)<capacity;
}
// Host-testable JSON serialization. Keep HTTP's output buffer bounded and
// reject truncation rather than returning incomplete diagnostic evidence.
inline bool formatAuditJson(const Snapshot& s,uint32_t count,
                            char* output,size_t capacity) {
  if(!output || capacity==0)return false;
  const int n=snprintf(output,capacity,
    "{\"ready\":%s,\"read_only\":true,\"uptime_ms\":%lu,\"sample_count\":%lu,"
    "\"diagnosis\":\"%s\",\"uniform_data\":%s,\"identifiers_ok\":%s,"
    "\"config_readback_matches\":%s,"
    "\"part\":%u,\"version\":%u,\"marc_raw\":%u,"
    "\"iocfg2_first\":%u,\"iocfg2_second\":%u,"
    "\"pktctrl0_first\":%u,\"pktctrl0_second\":%u,"
    "\"mdmcfg2\":%u,\"freq2\":%u,\"freq1\":%u,\"freq0\":%u,"
    "\"chip_status_at_part_header\":%u,\"chip_status_at_marc_header\":%u,\"chip_status_at_io2_header\":%u,"
    "\"freq_word\":%lu,\"expected_freq_word\":%lu,"
    "\"note\":\"Raw SPI reads only; no reset, strobe, RF TX or pairing\"}",
    s.sampled?"true":"false",
    static_cast<unsigned long>(s.uptime_ms),
    static_cast<unsigned long>(count),s.diagnosis(),
    s.uniform_data()?"true":"false",
    s.identifiers_ok()?"true":"false",
    s.config_ok()?"true":"false",
    static_cast<unsigned>(s.part),static_cast<unsigned>(s.version),
    static_cast<unsigned>(s.marc_raw),
    static_cast<unsigned>(s.iocfg2_a),static_cast<unsigned>(s.iocfg2_b),
    static_cast<unsigned>(s.pktctrl0_a),static_cast<unsigned>(s.pktctrl0_b),
    static_cast<unsigned>(s.mdmcfg2),
    static_cast<unsigned>(s.freq2),static_cast<unsigned>(s.freq1),
    static_cast<unsigned>(s.freq0),
    static_cast<unsigned>(s.part_header),
    static_cast<unsigned>(s.marc_header),
    static_cast<unsigned>(s.io2_header),
    static_cast<unsigned long>(s.received_freq_word()),
    static_cast<unsigned long>(s.expected_freq_word));
  return n>=0 && static_cast<size_t>(n)<capacity;
}
} // namespace ramses_spi_audit
