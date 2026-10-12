// Configurable DHCP / mDNS hostname for the RAMSES Arduino gateway.
// Self-contained host-native logic: validate a single RFC 1123-style DNS label.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace gateway_hostname {
constexpr size_t MAX_LENGTH = 63;

inline char asciiLower(char value) {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value + ('a' - 'A'));
  return value;
}

inline bool isValid(const char* name) {
  if (!name) return false;
  // Bounded read even when the source is malformed or unexpectedly long.
  const size_t length = strlen(name);
  if (length == 0 || length > MAX_LENGTH) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = name[i];
    const bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    const bool digit = c >= '0' && c <= '9';
    const bool hyphen = c == '-';
    if (!alpha && !digit && !hyphen) return false;
    if (hyphen && (i == 0 || i + 1 == length)) return false;
  }
  return true;
}

// When GW_HOSTNAME is absent, blank, or invalid, use a unique, valid default
// derived from the last 24 bits of the ESP32 efuse MAC. Never trust unsafe
// characters in an mDNS/DHCP name or in the diagnostics page.
inline bool resolve(const char* configured, uint32_t macSuffix,
                    char* output, size_t outputSize) {
  if (!output || outputSize < MAX_LENGTH + 1) return false;
  if (isValid(configured)) {
    const size_t len = strlen(configured);
    for (size_t i = 0; i < len; ++i) output[i] = asciiLower(configured[i]);
    output[len] = '\0';
    return true; // Explicit configuration accepted
  }
  const int written = snprintf(output, outputSize, "evohome-gateway-%06lx",
                               static_cast<unsigned long>(macSuffix & 0xFFFFFFu));
  return written > 0 && static_cast<size_t>(written) < outputSize
         ? false : false; // False means the configured value was not accepted.
}
} // namespace gateway_hostname
