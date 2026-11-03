#pragma once

#include <stdint.h>

// Decision logic for safe mode, free of Arduino types so tests/ can compile it
// on a host. safe_mode.h holds the RTC storage and the boot hook.

// Consecutive crashed starts before the clock gives up on normal operation.
constexpr uint32_t SAFE_MODE_AFTER_CRASHES = 3;

// Values of the SDK's rst_reason enum (safe_mode.h static_asserts they match).
constexpr int RESET_WDT = 1;
constexpr int RESET_EXCEPTION = 2;
constexpr int RESET_SOFT_WDT = 3;

inline bool isCrashReset(int reason) {
  return reason == RESET_WDT || reason == RESET_EXCEPTION || reason == RESET_SOFT_WDT;
}

// Count of consecutive crashed starts including this one. Any clean start
// (power on, button, OTA/software restart, deep sleep) resets it. `valid` is
// false when the stored count is missing or corrupt, which counts as zero.
inline uint32_t nextCrashCount(bool valid, uint32_t previous, int reason) {
  if (!isCrashReset(reason)) {
    return 0;
  }
  return (valid ? previous : 0) + 1;
}

inline bool shouldEnterSafeMode(uint32_t crashCount) {
  return crashCount >= SAFE_MODE_AFTER_CRASHES;
}
