#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../RamsesGateway/GatewayDiscovery.h"

int main() {
  namespace d = gateway_discovery;
  assert(d::ENTITY_COUNT == 8);
  assert(d::validGatewayId("18:447669"));
  assert(!d::validGatewayId("18:444"));
  assert(!d::validGatewayId("18:123456/garbage"));
  assert(!d::validGatewayId("../secret"));
  for (size_t i = 0; i < d::ENTITY_COUNT; ++i) {
    char topic[144] = {};
    char payload[1080] = {};
    assert(d::buildConfig(i, "18:447669", "RAMSES/GATEWAY/18:447669",
           "0.3.0", topic, sizeof(topic), payload, sizeof(payload)));
    assert(strstr(topic, "homeassistant/") == topic);
    assert(strstr(topic, "/ramses_gateway_18_447669/") != nullptr);
    assert(strstr(payload, "\"identifiers\":[\"ramses_gateway_18_447669\"]") != nullptr);
    assert(strstr(payload, "\"availability_topic\":\"RAMSES/GATEWAY/18:447669\"") != nullptr);
    assert(strstr(payload, "\"state_topic\":\"RAMSES/GATEWAY/18:447669/status\"") != nullptr);
    assert(strstr(payload, "\"sw_version\":\"0.3.0\"") != nullptr);
    assert(strstr(payload, "\"entity_category\":\"diagnostic\"") != nullptr);
    assert(strstr(payload, "\"unique_id\":\"ramses_gateway_18_447669_") != nullptr);
    if (i == 0) assert(strstr(payload, "\"payload_on\":\"ON\"") != nullptr);
    char shortBuffer[50] = {};
    assert(!d::buildConfig(i, "18:447669", "RAMSES/GATEWAY/18:447669",
           "0.3.0", topic, sizeof(topic), shortBuffer, sizeof(shortBuffer)));
    // Machine-readable records checked by Python json.loads in CI.
    printf("%s\t%s\n", topic, payload);
  }
  return 0;
}
