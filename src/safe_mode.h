#pragma once

#include "safe_mode_logic.h"
#include "vars.h"

/*
 * Boot-loop protection. A firmware that crashes at startup can't be fixed over
 * OTA (it never stays up long enough), which would need a USB cable. Crashed
 * starts (exception or watchdog reset) are counted in RTC memory, which
 * survives those resets but not a power cycle. After SAFE_MODE_AFTER_CRASHES in
 * a row the clock starts in safe mode: WiFi, a status page and OTA only, no
 * display, sensors, Home Assistant or NTP. Flashing a fixed firmware over OTA
 * (a clean software restart) or pressing "Try a normal start" resets the count.
 */

static_assert(RESET_WDT == REASON_WDT_RST && RESET_EXCEPTION == REASON_EXCEPTION_RST &&
              RESET_SOFT_WDT == REASON_SOFT_WDT_RST, "crash reasons must match the SDK's rst_reason");

struct RtcCrashState {
  uint32_t magic;
  uint32_t count;
  uint32_t check;  // count ^ magic, so stale RTC garbage isn't mistaken for a count
};
constexpr uint32_t RTC_CRASH_MAGIC = 0x5AFE0C01;
constexpr uint32_t RTC_CRASH_OFFSET = 0;  // in 4-byte blocks

bool readCrashState(uint32_t &count) {
  RtcCrashState s;
  if (!ESP.rtcUserMemoryRead(RTC_CRASH_OFFSET, (uint32_t *)&s, sizeof(s))) {
    return false;
  }
  if (s.magic != RTC_CRASH_MAGIC || s.check != (s.count ^ RTC_CRASH_MAGIC)) {
    return false;
  }
  count = s.count;
  return true;
}

void writeCrashState(uint32_t count) {
  RtcCrashState s = {RTC_CRASH_MAGIC, count, count ^ RTC_CRASH_MAGIC};
  ESP.rtcUserMemoryWrite(RTC_CRASH_OFFSET, (uint32_t *)&s, sizeof(s));
}

// Call first thing in setup(). Returns true when this start must run in safe mode.
bool safeModeBoot() {
  uint32_t previous = 0;
  bool valid = readCrashState(previous);
  crashCount = nextCrashCount(valid, previous, ESP.getResetInfoPtr()->reason);
  writeCrashState(crashCount);
  safeMode = shouldEnterSafeMode(crashCount);
  return safeMode;
}

// A normal start that has run this long without crashing counts as healthy.
const unsigned long SAFE_MODE_STABLE_MS = 120000;
bool crashCountCleared = false;

void markStableIfDue() {
  if (!crashCountCleared && millis() >= SAFE_MODE_STABLE_MS) {
    crashCountCleared = true;
    crashCount = 0;
    writeCrashState(0);
  }
}

// Diagnostics hook (POST /debug/crash) so safe mode can be exercised on a real
// clock: an exception reset, or a hang that the software watchdog resets.
void crashNow(bool hang) {
  if (hang) {
    for (;;) {
      __asm__ __volatile__("nop");
    }
  }
  volatile int *p = nullptr;
  *p = 1;
}
