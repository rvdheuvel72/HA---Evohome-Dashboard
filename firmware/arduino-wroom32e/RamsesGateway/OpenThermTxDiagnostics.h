// OpenTherm one-shot TX diagnostics, no Arduino dependencies.
// A local radio result does NOT demonstrate that the RF frame was heard by R8810A.
#pragma once
#include <stdint.h>
namespace ot_tx {
enum Method : uint8_t { ASYNC_UART=0, REFERENCE_FIFO=1 };
inline const char* methodName(Method m) {
  return m==REFERENCE_FIFO?"fifo_uartframed_55":"async_uart";
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
  // Limits PA level to a conservative nominal -10 dBm at 868 MHz.
  // A 433-MHz-matched board's true radiated emissions remain uncalibrated.
  static constexpr uint8_t PA_868_LOW=0x26;
  static constexpr uint32_t MAX_FIFO_TX_US=80000;
};
struct Comparison {
  uint32_t attempts=0, local_completed=0, read_acks=0,
           timeouts=0, rejected=0, tx_failures=0;
  Phase last_phase=NOT_STARTED;
  uint32_t last_elapsed_us=0;
};
} // namespace ot_tx
