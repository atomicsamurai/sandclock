// Host test for src/date_screen_logic.h (the same header the firmware uses).
// Built and run by tests/run.sh.
#include <cstdio>
#include <cstring>

#include "../src/date_screen_logic.h"

static int failures = 0;

static void expect(const char *name, bool pass) {
  printf("  %-40s %s\n", name, pass ? "pass" : "FAIL");
  failures += !pass;
}

static bool layout(int wday, int mday, const char *text, int x) {
  char out[8];
  int got = dateLayout(wday, mday, out);
  bool ok = strcmp(out, text) == 0 && got == x;
  if (!ok) printf("    got \"%s\" at x=%d, want \"%s\" at x=%d\n", out, got, text, x);
  return ok;
}

int main() {
  expect("two-digit day fills all 32 columns", layout(4, 19, "THU 19", 0));
  expect("one-digit day is centered", layout(1, 5, "MON 5", 3));
  expect("Sunday is 0", layout(0, 1, "SUN 1", 3));
  expect("Saturday is 6", layout(6, 31, "SAT 31", 0));
  expect("bad weekday falls back to SUN", layout(9, 12, "SUN 12", 0));
  expect("bad day falls back to 1", layout(2, 0, "TUE 1", 3));
  expect("width of THU 19", dateTextWidth("THU 19") == 32);
  expect("width fits the panel for every day", [] {
    char out[8];
    for (int w = 0; w < 7; w++)
      for (int d = 1; d <= 31; d++) {
        int x = dateLayout(w, d, out);
        if (x < 0 || x + dateTextWidth(out) > DATE_PANEL_WIDTH) return false;
      }
    return true;
  }());
  expect("not due at :29", !dateScreenDue(29));
  expect("due at :30 :31 :32", dateScreenDue(30) && dateScreenDue(31) && dateScreenDue(32));
  expect("not due at :33 or :00", !dateScreenDue(33) && !dateScreenDue(0));

  printf(failures ? "%d FAILED\n" : "all passed\n", failures);
  return failures ? 1 : 0;
}
