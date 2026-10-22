// Passive CC1101 RX-health observations; populated solely by radioTask.
#pragma once
#include <stdint.h>
namespace ramses_rx_health {
constexpr uint8_t RX_STATE=0x0D;
constexpr uint8_t EXPECTED_IOCFG2=0x0D;
constexpr uint8_t EXPECTED_PKTCTRL0=0x32;
struct Sample {
  uint32_t uptime_ms=0;
  uint32_t uart_bytes=0;
  uint8_t marcstate=0xFF;
  uint8_t iocfg2=0xFF;
  uint8_t pktctrl0=0xFF;
  uint8_t gdo2_level=0xFF;
  bool inRx() const { return marcstate==RX_STATE; }
  bool settingsMatch() const {
    return iocfg2==EXPECTED_IOCFG2 && pktctrl0==EXPECTED_PKTCTRL0;
  }
};
struct Report {
  Sample latest={},before_recalibration={},after_recalibration={};
  uint32_t samples=0;
  uint32_t samples_not_in_rx=0;
  uint32_t samples_config_mismatch=0;
  uint32_t gpio_high_samples=0;
  uint32_t gpio_low_samples=0;
  uint32_t recalibrations_sampled=0;
  uint32_t recalibrations_not_in_rx=0;
  bool has_latest=false;
  bool has_recalibration=false;
  bool last_recalibration_spi_ok=false;
  void observe(const Sample& s) {
    latest=s;
    has_latest=true;
    ++samples;
    if(!s.inRx())++samples_not_in_rx;
    if(!s.settingsMatch())++samples_config_mismatch;
    if(s.gdo2_level==1)++gpio_high_samples;
    if(s.gdo2_level==0)++gpio_low_samples;
  }
  void recalibration(const Sample& before,const Sample& after,bool spi_ok) {
    before_recalibration=before;
    after_recalibration=after;
    has_recalibration=true;
    last_recalibration_spi_ok=spi_ok;
    ++recalibrations_sampled;
    if(!after.inRx())++recalibrations_not_in_rx;
    observe(after);
  }
};
} // namespace ramses_rx_health
