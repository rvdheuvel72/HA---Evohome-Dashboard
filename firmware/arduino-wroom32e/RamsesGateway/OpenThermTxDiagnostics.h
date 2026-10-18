// OpenTherm one-shot TX diagnostics, no Arduino dependencies.
// A local radio result does NOT demonstrate that the RF frame was heard by R8810A.
#pragma once
#include <stdint.h>
#include <string.h>
namespace ot_tx {
enum Method : uint8_t { ASYNC_UART=0, REFERENCE_FIFO=1 };
inline const char* methodName(Method m) {
  return m==REFERENCE_FIFO?"fifo_reference_primed_60":"async_uart";
}
enum Phase : uint8_t {
  NOT_STARTED, FREQUENCY_REJECTED, RADIO_IDLE_FAILED, PA_PROGRAM_FAILED,
  TX_CONFIG_FAILED, FIFO_PREFILL_FAILED, TX_START_FAILED, TX_TIMEOUT,
  TX_UNDERFLOW, UART_WRITE_FAILED, RX_RESTORE_FAILED, LOCAL_COMPLETE
};
inline const char* phaseName(Phase p) {
  switch(p) {
    case NOT_STARTED:return "not_started";
    case FREQUENCY_REJECTED:return "frequency_rejected";
    case RADIO_IDLE_FAILED:return "radio_idle_failed";
    case PA_PROGRAM_FAILED:return "pa_program_failed";
    case TX_CONFIG_FAILED:return "tx_config_failed";
    case FIFO_PREFILL_FAILED:return "fifo_prefill_failed";
    case TX_START_FAILED:return "tx_start_failed";
    case TX_TIMEOUT:return "tx_timeout";
    case TX_UNDERFLOW:return "tx_underflow";
    case UART_WRITE_FAILED:return "uart_write_failed";
    case RX_RESTORE_FAILED:return "rx_restore_failed";
    case LOCAL_COMPLETE:return "local_complete";
  }
  return "unknown";
}
struct Report {
  Method method=ASYNC_UART;
  Phase phase=NOT_STARTED;
  uint8_t pa_configured=0, pa_readback=0;
  uint8_t marc_before=0xff,marc_during=0xff,marc_after=0xff;
  uint8_t txbytes_before=0xff,txbytes_after=0xff;
  uint16_t expected_bytes=0,written_bytes=0;
  uint32_t elapsed_us=0;
  bool rx_restored=false, saw_tx=false;
  // Extra observations for the manual 1FC9 async-UART TX path only.
  // Values are SPI readbacks: they do not prove data crossed the antenna.
  bool verification_measured=false, rx_state_confirmed=false;
  uint8_t pktctrl0_readback=0xff, iocfg0_readback=0xff;
  uint8_t mdmcfg4_readback=0xff, mdmcfg3_readback=0xff, mdmcfg2_readback=0xff;
  uint8_t marc_after_uart=0xff;
  uint32_t frequency_expected=0, frequency_readback=0;
  uint32_t uart_elapsed_us=0, uart_min_expected_us=0;
  // Limits PA level to a conservative nominal -10 dBm at 868 MHz.
  // A 433-MHz-matched board's true radiated emissions remain uncalibrated.
  static constexpr uint8_t PA_868_LOW=0x26;
  static constexpr uint32_t MAX_FIFO_TX_US=80000;
};
// A strict, fail-closed local-only verdict. Never call this "RF delivered".
inline const char* localVerificationReason(const Report& d) {
  if(!d.verification_measured)return "not_measured";
  if(d.phase!=LOCAL_COMPLETE)return "tx_phase_failed";
  if(!d.frequency_expected ||
     d.frequency_expected!=d.frequency_readback)return "frequency_readback_mismatch";
  // 0x32: asynchronous serial PKTCTRL0; 0x2E: GDO0 high-Z input.
  if(d.pktctrl0_readback!=0x32 || d.iocfg0_readback!=0x2E)
    return "async_data_pin_config_mismatch";
  // IndaloTech ramses_esp 38.4kbps GFSK data-rate/modulation register values.
  if(d.mdmcfg4_readback!=0x6A || d.mdmcfg3_readback!=0x83 ||
     d.mdmcfg2_readback!=0x10)return "modem_register_mismatch";
  if(d.pa_readback!=d.pa_configured ||
     d.pa_configured!=Report::PA_868_LOW)return "pa_readback_mismatch";
  if(!d.saw_tx || d.marc_after_uart!=0x13u)return "not_in_tx_after_uart_flush";
  if(!d.expected_bytes || d.written_bytes!=d.expected_bytes)
    return "uart_length_mismatch";
  // UART is 38400 baud, 8N1. Keep a small allowance for clock error and
  // task scheduling; a much shorter interval implies bytes weren't clocked.
  if(!d.uart_min_expected_us ||
     d.uart_elapsed_us<d.uart_min_expected_us)return "uart_duration_too_short";
  if(!d.rx_restored || !d.rx_state_confirmed)
    return "rx_state_not_confirmed";
  return "local_tx_verified";
}
inline bool localVerified(const Report& d) {
  return strcmp(localVerificationReason(d),"local_tx_verified")==0;
}
struct Comparison {
  uint32_t attempts=0, local_completed=0, read_acks=0,
           timeouts=0, rejected=0, tx_failures=0;
  Phase last_phase=NOT_STARTED;
  uint32_t last_elapsed_us=0;
};
} // namespace ot_tx
