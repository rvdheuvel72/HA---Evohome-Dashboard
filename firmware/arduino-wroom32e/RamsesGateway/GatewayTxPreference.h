// Persistent, gateway-owned preference for the experimental ID25 one-shot
// transmission. Independent NVS key avoids rewriting networking credentials
// and the staged/reboot-based general settings transaction.
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <stdint.h>

namespace gw_tx_pref {
constexpr uint8_t FIFO=1;
constexpr uint8_t UART=0;
inline const char* label(uint8_t method) {
  return method==UART?"uart":"fifo";
}
inline bool valid(const String& method) {
  return method=="fifo" || method=="uart";
}
inline uint8_t read() {
  Preferences p;
  if(!p.begin("ot-tx-method",true))return FIFO; // Fail safe to validated FIFO
  const uint8_t stored=p.getUChar("method",FIFO);
  p.end();
  return stored==UART?UART:FIFO; // Unknown/corrupt value -> FIFO
}
inline bool write(const String& selected) {
  if(!valid(selected))return false;
  const uint8_t next=selected=="uart"?UART:FIFO;
  Preferences p;
  if(!p.begin("ot-tx-method",false))return false;
  const bool ok=p.putUChar("method",next)==1 &&
                p.getUChar("method",255)==next;
  p.end();
  return ok;
}
}
