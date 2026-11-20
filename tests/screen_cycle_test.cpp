// Host test for src/screen_cycle_logic.h (the same header the firmware uses).
// Built and run by tests/run.sh.
#include <cstdio>
#include <cstring>

#include "../src/screen_cycle_logic.h"

static int failures = 0;

static void expect(const char *name, bool pass) {
  printf("  %-48s %s\n", name, pass ? "pass" : "FAIL");
  failures += !pass;
}

// Steps once a second, as loop() does, from t=1000 ms for `secs` seconds and
// writes one letter per second: T(ime/combined), t(emp), D(ate).
static void run(uint8_t style, uint8_t mode, int secs, char *out) {
  ScreenCycle c;
  c.startMs = 1000;
  for (int i = 0; i < secs; i++) {
    uint8_t s = c.step(1000 + i * 1000UL, style, mode);
    out[i] = s == SCREEN_MAIN ? 'T' : s == SCREEN_TEMP ? 't' : 'D';
  }
  out[secs] = 0;
}

static bool seq(uint8_t style, uint8_t mode, const char *want) {
  char got[64];
  run(style, mode, (int)strlen(want), got);
  bool ok = strcmp(got, want) == 0;
  if (!ok) printf("    got  %s\n    want %s\n", got, want);
  return ok;
}

int main() {
  expect("cycle, date off: time 5 s, temp 2 s", seq(1, DATE_MODE_OFF, "TTTTTttTTTTTtt"));
  expect("cycle, date once a minute: same as off", seq(1, DATE_MODE_MINUTE, "TTTTTttTTTTTtt"));
  expect("cycle, date in cycle: time, temp, date", seq(1, DATE_MODE_CYCLE, "TTTTTttDDTTTTTttDD"));
  expect("combined, date off: never changes", seq(2, DATE_MODE_OFF, "TTTTTTTTTTTT"));
  expect("combined, date in cycle: combined 5 s, date 2 s", seq(2, DATE_MODE_CYCLE, "TTTTTDDTTTTTDD"));

  expect("next after temp, date off, wraps to main", nextScreen(SCREEN_TEMP, 1, DATE_MODE_OFF) == SCREEN_MAIN);
  expect("next after main, combined + cycle, is date", nextScreen(SCREEN_MAIN, 2, DATE_MODE_CYCLE) == SCREEN_DATE);
  expect("next after date wraps to main", nextScreen(SCREEN_DATE, 1, DATE_MODE_CYCLE) == SCREEN_MAIN);

  {
    ScreenCycle c;
    c.screen = SCREEN_TEMP;
    c.startMs = 0;
    expect("temp screen ends at once on switch to combined", c.step(500, 2, DATE_MODE_OFF) == SCREEN_MAIN && c.startMs == 500);
  }
  {
    ScreenCycle c;
    c.screen = SCREEN_DATE;
    c.startMs = 0;
    expect("date screen ends at once when date mode leaves cycle", c.step(500, 1, DATE_MODE_MINUTE) == SCREEN_MAIN);
  }
  {
    ScreenCycle c;
    c.startMs = 0xFFFFF000UL;  // millis() wraps after ~49 days
    expect("millis() wrap keeps timing", c.step(0xFFFFF000UL + 4000, 1, DATE_MODE_OFF) == SCREEN_MAIN &&
                                             c.step(0x00000388UL, 1, DATE_MODE_OFF) == SCREEN_TEMP);
  }

  expect("dateMode wins over dateEnabled", dateModeFrom(true, 2, true, false) == DATE_MODE_CYCLE);
  expect("dateMode 0 is off", dateModeFrom(true, 0, false, false) == DATE_MODE_OFF);
  expect("out-of-range dateMode means once a minute", dateModeFrom(true, 7, false, false) == DATE_MODE_MINUTE);
  expect("negative dateMode means once a minute", dateModeFrom(true, -1, false, false) == DATE_MODE_MINUTE);
  expect("old dateEnabled true is once a minute", dateModeFrom(false, 0, true, true) == DATE_MODE_MINUTE);
  expect("old dateEnabled false is off", dateModeFrom(false, 0, true, false) == DATE_MODE_OFF);
  expect("neither key: once a minute", dateModeFrom(false, 0, false, false) == DATE_MODE_MINUTE);

  printf("%s\n", failures ? "screen cycle tests FAILED" : "screen cycle tests passed");
  return failures ? 1 : 0;
}
