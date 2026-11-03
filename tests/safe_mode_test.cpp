// Host test for src/safe_mode_logic.h. Built and run by tests/run.sh.
#include <cstdio>
#include <initializer_list>

#include "../src/safe_mode_logic.h"

static int failures = 0;

static void expect(const char *name, bool ok) {
  printf("  %-52s %s\n", name, ok ? "pass" : "FAIL");
  failures += !ok;
}

int main() {
  // SDK rst_reason: 0 power on, 1 hw wdt, 2 exception, 3 soft wdt, 4 software restart, 5 deep sleep, 6 external
  for (int reason : {0, 4, 5, 6}) {
    char name[64];
    snprintf(name, sizeof(name), "clean reset %d clears the count", reason);
    expect(name, nextCrashCount(true, 2, reason) == 0);
  }
  for (int reason : {1, 2, 3}) {
    char name[64];
    snprintf(name, sizeof(name), "crash reset %d counts one more", reason);
    expect(name, nextCrashCount(true, 1, reason) == 2);
  }
  expect("a crash with no valid stored count starts at 1", nextCrashCount(false, 99, 2) == 1);
  expect("a clean start with no valid stored count is 0", nextCrashCount(false, 99, 0) == 0);

  // Walk a boot loop: crash, crash, crash enters safe mode on the third start.
  uint32_t count = 0;
  bool valid = false;
  bool safe[4];
  for (int i = 0; i < 4; i++) {
    count = nextCrashCount(valid, count, 2);
    valid = true;
    safe[i] = shouldEnterSafeMode(count);
  }
  expect("boot loop: starts 1 and 2 are normal", !safe[0] && !safe[1]);
  expect("boot loop: start 3 enters safe mode", safe[2]);
  expect("boot loop: start 4 stays in safe mode", safe[3]);

  // Recovery: an OTA flash restarts the clock cleanly.
  expect("OTA restart after safe mode returns to normal",
         !shouldEnterSafeMode(nextCrashCount(true, count, 4)));
  // A crash after healthy running starts a fresh count, not a safe-mode entry.
  expect("an isolated crash is not safe mode", !shouldEnterSafeMode(nextCrashCount(true, 0, 2)));
  expect("threshold is 3", !shouldEnterSafeMode(2) && shouldEnterSafeMode(3));

  printf("safe_mode: %d failures\n", failures);
  return failures != 0;
}
