#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../RamsesGateway/OpenThermTxDiagnostics.h"

static ot_tx::Report valid() {
  ot_tx::Report d{};
  d.verification_measured=true;
  d.method=ot_tx::ASYNC_UART;
  d.phase=ot_tx::LOCAL_COMPLETE;
  d.frequency_expected=0x21656A;
  d.frequency_readback=0x21656A;
  d.pktctrl0_readback=0x32;
  d.iocfg0_readback=0x2E;
  d.mdmcfg4_readback=0x6A;
  d.mdmcfg3_readback=0x83;
  d.mdmcfg2_readback=0x10;
  d.pa_configured=ot_tx::Report::PA_868_LOW;
  d.pa_readback=ot_tx::Report::PA_868_LOW;
  d.saw_tx=true;
  d.marc_during=0x13;
  d.marc_after_uart=0x13;
  d.marc_after=0x0D;
  d.expected_bytes=70;
  d.written_bytes=70;
  d.uart_elapsed_us=18230;
  d.uart_min_expected_us=16406;
  d.rx_restored=true;
  d.rx_state_confirmed=true;
  return d;
}
static void check(const ot_tx::Report& d,const char* reason){
  assert(strcmp(ot_tx::localVerificationReason(d),reason)==0);
  assert(ot_tx::localVerified(d)==(strcmp(reason,"local_tx_verified")==0));
}
int main() {
  ot_tx::Report d{};
  check(d,"not_measured");
  d=valid();check(d,"local_tx_verified");
  d=valid();d.phase=ot_tx::RX_RESTORE_FAILED;check(d,"tx_phase_failed");
  d=valid();d.frequency_readback^=1;check(d,"frequency_readback_mismatch");
  d=valid();d.pktctrl0_readback=0x00;check(d,"async_data_pin_config_mismatch");
  d=valid();d.iocfg0_readback=0x00;check(d,"async_data_pin_config_mismatch");
  d=valid();d.mdmcfg4_readback=0;check(d,"modem_register_mismatch");
  d=valid();d.mdmcfg3_readback=0;check(d,"modem_register_mismatch");
  d=valid();d.mdmcfg2_readback=0;check(d,"modem_register_mismatch");
  d=valid();d.pa_readback=0;check(d,"pa_readback_mismatch");
  d=valid();d.saw_tx=false;check(d,"not_in_tx_after_uart_flush");
  d=valid();d.marc_after_uart=0x01;check(d,"not_in_tx_after_uart_flush");
  d=valid();d.written_bytes=69;check(d,"uart_length_mismatch");
  d=valid();d.uart_elapsed_us=16000;check(d,"uart_duration_too_short");
  d=valid();d.rx_restored=false;check(d,"rx_state_not_confirmed");
  d=valid();d.rx_state_confirmed=false;check(d,"rx_state_not_confirmed");
  puts("Strict local CC1101 TX verification: positives and fail-closed negatives PASS");
}
