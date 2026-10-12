// Bounded diagnostic ring, only mutated from Arduino setup()/loop().
// Pure C++ for deterministic host-side testing.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

class GatewayDebug {
 public:
  enum Level : uint8_t { ERROR = 0, WARN = 1, INFO = 2, TRACE = 3 };
  static constexpr size_t CAPACITY = 64;
  static constexpr size_t LINE_BYTES = 144;
  struct Entry {
    uint32_t sequence = 0;
    uint32_t uptimeMs = 0;
    Level level = INFO;
    char message[LINE_BYTES] = {};
  };

  static const char* levelName(Level level) {
    switch (level) {
      case ERROR: return "ERROR";
      case WARN: return "WARN";
      case INFO: return "INFO";
      case TRACE: return "TRACE";
      default: return "UNKNOWN";
    }
  }
  void add(uint32_t uptimeMs, Level level, const char* message) {
    if (!message || level > TRACE) return;
    Entry& entry = entries_[nextIndex_];
    entry.sequence = ++sequence_;
    entry.uptimeMs = uptimeMs;
    entry.level = level;
    snprintf(entry.message, sizeof(entry.message), "%s", message);
    nextIndex_ = (nextIndex_ + 1) % CAPACITY;
    if (count_ < CAPACITY) ++count_;
  }
  template <typename Callback>
  void eachAfter(uint32_t cursor, Level maximum, Callback callback) const {
    const size_t oldest = (nextIndex_ + CAPACITY - count_) % CAPACITY;
    for (size_t i = 0; i < count_; ++i) {
      const Entry& entry = entries_[(oldest + i) % CAPACITY];
      if (entry.sequence > cursor && entry.level <= maximum) callback(entry);
    }
  }
  uint32_t newest() const { return sequence_; }
  size_t count() const { return count_; }

 private:
  Entry entries_[CAPACITY] = {};
  uint32_t sequence_ = 0;
  size_t nextIndex_ = 0, count_ = 0;
};
