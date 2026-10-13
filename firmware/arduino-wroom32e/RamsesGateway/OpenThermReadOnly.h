// Read-only OpenTherm over RAMSES-II (3220). Arduino-independent.
// "Read-Data" is the ONLY permitted OpenTherm request message type.
// Support probing is not evidence of boiler compatibility until replies are observed.
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ot_readonly {
enum Result : uint8_t { NOT_TESTED=0, READ_ACK=1, DATA_INVALID=2, UNKNOWN_ID=3, TIMEOUT=4 };
struct Entry {
  Result result = NOT_TESTED;
  uint16_t raw = 0;
  uint32_t receivedAtMs = 0;
};
inline uint8_t parity(uint32_t v) {
  uint8_t p=0;
  while (v) { p ^= static_cast<uint8_t>(v & 1u); v >>= 1; }
  return p;
}
// Wrapper: 00 + OpenTherm frame (P, message type, Data-ID, 16-bit value).
inline bool request(uint16_t id, uint8_t out[5]) {
  if (!out || id > 255) return false;
  // Zero data field. Message type is ALWAYS 000 (Read-Data).
  out[0]=0; out[1]=parity(static_cast<uint32_t>(id) << 16) ? 0x80 : 0;
  out[2]=static_cast<uint8_t>(id); out[3]=0; out[4]=0;
  return true;
}
inline bool response(const uint8_t* data, size_t len, uint8_t& id, Result& result, uint16_t& raw) {
  if (!data || len != 5 || data[0] != 0 || (data[1]&0x0f) != 0) return false;
  const uint32_t frame=(static_cast<uint32_t>(data[1])<<24)
      |(static_cast<uint32_t>(data[2])<<16)
      |(static_cast<uint32_t>(data[3])<<8)|data[4];
  if (parity(frame)) return false; // total parity must be even
  const uint8_t type=(data[1]>>4)&7u;
  if (type==4) result=READ_ACK;
  else if (type==6) result=DATA_INVALID;
  else if (type==7) result=UNKNOWN_ID;
  else return false; // a Write-Ack is never a valid answer to our test
  id=data[2]; raw=(static_cast<uint16_t>(data[3])<<8)|data[4];
  return true;
}
inline const char* label(uint8_t id) {
  switch (id) {
    case 0:return "status";
    case 3:return "slave_configuration";
    case 5:return "fault_flags";
    case 6:return "remote_parameter_flags";
    case 10:return "transparent_parameter_count";
    case 12:return "fault_history_size";
    case 17:return "modulation_percent";
    case 18:return "water_pressure_bar";
    case 19:return "dhw_flow_l_min";
    case 25:return "boiler_flow_c";
    case 26:return "dhw_temperature_c";
    case 27:return "outside_temperature_c";
    case 28:return "boiler_return_c";
    case 33:return "exhaust_temperature_c";
    case 48:return "dhw_bounds";
    case 49:return "ch_bounds";
    case 56:return "dhw_setpoint_c";
    case 57:return "max_ch_setpoint_c";
    case 115:return "oem_diagnostic";
    case 116:return "burner_starts";
    case 117:return "ch_pump_starts";
    case 118:return "dhw_pump_starts";
    case 119:return "dhw_burner_starts";
    case 120:return "burner_hours";
    case 121:return "ch_pump_hours";
    case 122:return "dhw_pump_hours";
    case 123:return "dhw_burner_hours";
    case 127:return "slave_product_version";
    default:return "unmapped";
  }
}
// All ID 0..255 can be inspected with zero-valued Read-Data requests,
// but NOT all are read-eligible or meaningful with a zero data field.
// Only values explicitly acknowledged by the observed bridge become poll candidates.
// R8810A documented read candidates + extended diagnostics.
static constexpr uint8_t DEFAULT_IDS[]={
  0,3,5,6,10,12,17,18,19,25,26,27,28,33,48,49,56,57,
  115,116,117,118,119,120,121,122,123,127
};
static constexpr size_t DEFAULT_COUNT=sizeof(DEFAULT_IDS)/sizeof(DEFAULT_IDS[0]);
inline const char* state(Result r) {
  switch(r){case READ_ACK:return "read_ack";case DATA_INVALID:return "data_invalid";
   case UNKNOWN_ID:return "unknown_data_id";case TIMEOUT:return "timeout";
   default:return "not_tested";}
}
// Converts signed OpenTherm f8.8 to engineering units for IDs using f8.8.
inline bool fixedPoint(uint8_t id, uint16_t raw, float& value) {
  switch(id){
    case 17:case 18:case 19:case 25:case 26:case 27:
    case 28:case 33:case 56:case 57:
      value=static_cast<int16_t>(raw)/256.0f;return true;
    default:return false;
  }
}
inline bool pollable(uint8_t id) {
  // Do not automatically poll arbitrary/OEM/parameterized IDs, even if
  // the bridge reports a Read-Ack. These need a reviewed ID-specific parser.
  switch(id){
    case 0:case 3:case 5:case 6:case 17:case 18:case 19:case 25:
    case 26:case 27:case 28:case 33:case 56:case 57:return true;
    default:return false;
  }
}
} // namespace ot_readonly
