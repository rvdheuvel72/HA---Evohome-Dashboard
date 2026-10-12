#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../RamsesGateway/GatewayHostname.h"

int main() {
  char result[64] = {};
  assert(gateway_hostname::isValid("evohome-gateway"));
  assert(gateway_hostname::isValid("Host-9"));
  assert(gateway_hostname::isValid("a"));
  assert(!gateway_hostname::isValid(""));
  assert(!gateway_hostname::isValid(nullptr));
  assert(!gateway_hostname::isValid("-host"));
  assert(!gateway_hostname::isValid("host-"));
  assert(!gateway_hostname::isValid("has spaces"));
  assert(!gateway_hostname::isValid("has_underscore"));
  assert(!gateway_hostname::isValid("host.local")); // suffix is added by mDNS
  assert(!gateway_hostname::isValid("bad/name"));
  assert(!gateway_hostname::isValid("toolong----------------------------------------------------------------"));
  assert(gateway_hostname::resolve("EvoHome-Gateway", 0xC7DBC4, result, sizeof(result)));
  assert(strcmp(result, "evohome-gateway") == 0);
  assert(!gateway_hostname::resolve("", 0xC7DBC4, result, sizeof(result)));
  assert(strcmp(result, "evohome-gateway-c7dbc4") == 0);
  assert(!gateway_hostname::resolve("esp32_bad", 0x00A00B, result, sizeof(result)));
  assert(strcmp(result, "evohome-gateway-00a00b") == 0);
  char tooSmall[10] = {};
  assert(!gateway_hostname::resolve("valid", 0xAABBCC, tooSmall, sizeof(tooSmall)));
  assert(tooSmall[0] == '\0');
  char maxName[64];
  memset(maxName, 'a', 63);
  maxName[63] = '\0';
  assert(gateway_hostname::isValid(maxName));
  assert(gateway_hostname::resolve(maxName, 0, result, sizeof(result)));
  assert(strlen(result) == 63);
  char oversized[65];
  memset(oversized, 'b', 64);
  oversized[64] = '\0';
  assert(!gateway_hostname::isValid(oversized));
  puts("Hostname validation and deterministic fallback PASS");
  return 0;
}
