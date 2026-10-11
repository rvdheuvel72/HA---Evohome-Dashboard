#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include "../RamsesGateway/GatewayDebug.h"
int main() {
  GatewayDebug logs;
  assert(logs.newest() == 0 && logs.count() == 0);
  logs.add(1, GatewayDebug::INFO, "BOOT");
  logs.add(2, GatewayDebug::ERROR, "RADIO FAIL");
  logs.add(3, GatewayDebug::TRACE, "RF event");
  std::vector<std::string> values;
  logs.eachAfter(0, GatewayDebug::INFO, [&](const GatewayDebug::Entry &item) {
    values.emplace_back(item.message);
  });
  assert(values.size() == 2 && values[0] == "BOOT" && values[1] == "RADIO FAIL");
  values.clear();
  logs.eachAfter(1, GatewayDebug::WARN, [&](const GatewayDebug::Entry &item) {
    values.emplace_back(item.message);
  });
  assert(values.size() == 1 && values[0] == "RADIO FAIL");
  for (unsigned i=0; i<80; ++i) {
    char buffer[32];
    snprintf(buffer,sizeof(buffer), "EVENT %u",i);
    logs.add(i+100, GatewayDebug::INFO, buffer);
  }
  assert(logs.count() == GatewayDebug::CAPACITY);
  assert(logs.newest() == 83);
  std::vector<uint32_t> seqs;
  logs.eachAfter(0, GatewayDebug::TRACE, [&](const GatewayDebug::Entry &item) {
    seqs.push_back(item.sequence);
  });
  assert(seqs.size() == 64 && seqs.front() == 20 && seqs.back() == 83);
  std::string longMessage(700, 'x');
  logs.add(300, GatewayDebug::WARN, longMessage.c_str());
  const GatewayDebug::Entry* newest = nullptr;
  logs.eachAfter(83, GatewayDebug::TRACE, [&](const GatewayDebug::Entry &item) {
    newest = &item;
  });
  assert(newest && strlen(newest->message) == GatewayDebug::LINE_BYTES-1);
  puts("WEB DEBUG bounded ring tests PASS");
}