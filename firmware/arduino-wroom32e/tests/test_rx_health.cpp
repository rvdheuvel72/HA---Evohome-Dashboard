#include <cassert>
#include <cstdio>
#include "../RamsesGateway/RamsesRxHealth.h"
int main() {
  using namespace ramses_rx_health;
  Report r;
  assert(!r.has_latest && !r.has_recalibration && r.samples==0);
  Sample rx{};
  rx.uptime_ms=100;rx.uart_bytes=321;
  rx.marcstate=RX_STATE;rx.iocfg2=EXPECTED_IOCFG2;
  rx.pktctrl0=EXPECTED_PKTCTRL0;rx.gdo2_level=1;
  assert(rx.inRx() && rx.settingsMatch());
  r.observe(rx);
  assert(r.has_latest && r.samples==1 && r.samples_not_in_rx==0);
  assert(r.gpio_high_samples==1 && r.gpio_low_samples==0);
  Sample idle=rx;
  idle.uptime_ms=30000;idle.marcstate=0x01;idle.iocfg2=0xFF;idle.gdo2_level=0;
  assert(!idle.inRx() && !idle.settingsMatch());
  r.observe(idle);
  assert(r.samples==2 && r.samples_not_in_rx==1);
  assert(r.samples_config_mismatch==1 && r.gpio_low_samples==1);
  r.recalibration(idle,rx,true);
  assert(r.has_recalibration && r.last_recalibration_spi_ok);
  assert(r.recalibrations_sampled==1 && r.recalibrations_not_in_rx==0);
  assert(r.before_recalibration.marcstate==0x01);
  assert(r.after_recalibration.marcstate==RX_STATE);
  assert(r.samples==3 && r.latest.inRx());
  r.recalibration(rx,idle,true);
  assert(r.recalibrations_sampled==2 && r.recalibrations_not_in_rx==1);
  assert(r.last_recalibration_spi_ok && !r.after_recalibration.inRx());
  r.recalibration(idle,idle,false);
  assert(!r.last_recalibration_spi_ok && r.recalibrations_not_in_rx==2);
  // A static GPIO read is not a measurement of the RF waveform.
  assert(r.gpio_high_samples==2 && r.gpio_low_samples==3);
  RecoveryMonitor guard;
  assert(!guard.observe(rx,false)); // working RX: do nothing
  Sample bad=rx;
  bad.marcstate=0xFF; // failed SPI status read is not proof of IDLE
  assert(!guard.observe(bad,false));
  bad.marcstate=0x01;
  assert(!guard.observe(bad,false)); // first confirmed IDLE
  assert(guard.observe(bad,false)); // second consecutive IDLE
  guard.recovered();
  assert(!guard.observe(bad,true)); // active packet decoder blocks reentry
  assert(!guard.observe(bad,false));
  bad.marcstate=0x09; // CAL is transient
  assert(!guard.observe(bad,false));
  bad.marcstate=0x11; // RX FIFO overflow is actionable
  assert(!guard.observe(bad,false));
  assert(guard.observe(bad,false));
  guard.recovered();
  bad.iocfg2=0xFF; // CC1101 register readback changed; don't strobe blindly
  assert(!guard.observe(bad,false));
  assert(!guard.observe(bad,false));
  puts("CC1101 RX health snapshot tests PASS");
}
