// Host test for src/light_gesture_logic.h. Built and run by tests/run.sh.
#include <cstdio>
#include <initializer_list>
#include <utility>
#include <vector>

#include "../src/light_gesture_logic.h"

static int failures = 0;

static void expect(const char *name, bool ok) {
  printf("  %-56s %s\n", name, ok ? "pass" : "FAIL");
  failures += !ok;
}

// Drives a detector with a piecewise-constant light reading, one sample every
// GESTURE_SAMPLE_MS like the firmware. Returns how many times it fired.
struct Sim {
  LightGesture g;
  uint32_t t = 1000;
  int fired = 0;
  void hold(int level, uint32_t ms) {
    for (uint32_t end = t + ms; t < end; t += GESTURE_SAMPLE_MS) {
      fired += g.update(t, level);
    }
  }
  // n pulses at `pulse` level for `on` ms, back at `room` for `gap` ms
  void covers(int n, int room, int pulse, uint32_t on, uint32_t gap) {
    for (int i = 0; i < n; i++) {
      hold(pulse, on);
      hold(room, gap);
    }
  }
  void flashes(int n, int room, int flash, uint32_t on, uint32_t gap) { covers(n, room, flash, on, gap); }
};

static int run(std::initializer_list<std::pair<int, uint32_t>> steps, int warmLevel = 500) {
  Sim s;
  s.hold(warmLevel, 3000);
  for (auto [level, ms] : steps) s.hold(level, ms);
  return s.fired;
}

int main() {
  {
    Sim s;
    s.hold(500, 3000);
    s.covers(3, 500, 60, 400, 400);
    expect("three short covers fire once", s.fired == 1);
  }
  {
    Sim s;
    s.hold(120, 3000);  // dim evening room
    s.covers(3, 120, 20, 300, 300);
    expect("works in a dim room (relative threshold)", s.fired == 1);
  }
  {
    Sim s;
    s.hold(500, 3000);
    s.covers(2, 500, 60, 400, 400);
    s.hold(500, 5000);
    expect("two covers are not enough", s.fired == 0);
  }
  {
    Sim s;
    s.hold(500, 3000);
    s.covers(2, 500, 60, 400, 400);
    s.hold(500, 2000);  // pause too long before the third
    s.covers(1, 500, 60, 400, 400);
    expect("a long pause between covers starts over", s.fired == 0);
    s.covers(2, 500, 60, 400, 400);
    expect("...and three fresh covers then fire", s.fired == 1);
  }
  expect("lights off (stays dark) never fires", run({{30, 20000}}) == 0);
  expect("lights off then on never fires", run({{30, 5000}, {500, 5000}}) == 0);
  expect("lights off, on, off, on, off, on (slow) never fires",
         run({{30, 3000}, {500, 3000}, {30, 3000}, {500, 3000}, {30, 3000}, {500, 3000}}) == 0);
  {
    Sim s;
    s.hold(500, 3000);
    s.covers(3, 500, 60, 50, 400);  // 1-sample dips
    expect("flicker shorter than GESTURE_MIN_MS never fires", s.fired == 0);
  }
  {
    Sim s;
    s.hold(500, 3000);
    s.covers(3, 500, 60, 2500, 400);
    expect("covers held too long never fire", s.fired == 0);
  }
  {
    Sim s;
    s.hold(500, 3000);
    s.covers(3, 500, 300, 400, 400);  // 40% drop: a shadow, not a hand
    expect("a partial shadow (not below half) never fires", s.fired == 0);
  }
  {
    Sim s;
    s.hold(40, 3000);  // too dark to tell a hand from the room
    s.covers(3, 40, 0, 400, 400);
    expect("off when the room is darker than COVER_MIN_BASELINE", s.fired == 0);
  }
  {
    Sim s;
    s.hold(500, 3000);
    s.covers(3, 500, 60, 400, 400);
    s.covers(3, 500, 60, 400, 400);  // straight away again
    expect("cooldown: a repeat right away does not fire again", s.fired == 1);
    s.hold(500, GESTURE_COOLDOWN_MS);
    s.covers(3, 500, 60, 400, 400);
    expect("after the cooldown it fires again", s.fired == 2);
  }
  {
    Sim s;
    s.hold(500, 3000);
    for (int i = 0; i < 60; i++) s.hold(500 - i * 7, 1000);  // dusk over a minute
    s.covers(3, 80, 10, 400, 400);
    expect("follows a slow change in the room, then still fires", s.fired == 1);
  }
  {
    Sim s;
    s.hold(500, 3000);
    s.covers(4, 500, 60, 400, 400);
    expect("four covers fire once (the fourth is in the cooldown)", s.fired == 1);
  }

  // ---- Flashes (dark room) ----
  {
    Sim s;
    s.hold(10, 3000);  // dark bedroom
    s.flashes(3, 10, 900, 400, 400);
    expect("three flashes in a dark room fire once", s.fired == 1);
  }
  {
    Sim s;
    s.hold(150, 3000);  // dim room, flashlight from a distance
    s.flashes(3, 150, 450, 300, 500);
    expect("three flashes in a dim room fire", s.fired == 1);
  }
  {
    Sim s;
    s.hold(10, 3000);
    s.flashes(3, 10, 80, 400, 400);  // +70: a glow, not a flashlight
    expect("a weak glow (less than FLASH_MIN_RISE) never fires", s.fired == 0);
  }
  {
    Sim s;
    s.hold(500, 3000);
    s.flashes(3, 500, 800, 400, 400);  // lit room: +60% is not double
    expect("flashes in a lit room that do not double never fire", s.fired == 0);
  }
  expect("lights on (stays bright) never fires", run({{600, 20000}}, 10) == 0);
  expect("lights on, off, on, off, on, off (slow) never fires",
         run({{600, 3000}, {10, 3000}, {600, 3000}, {10, 3000}, {600, 3000}, {10, 3000}}, 10) == 0);
  {
    Sim s;
    s.hold(10, 3000);
    for (int i = 0; i < 3; i++) {  // headlights sweeping past, three cars
      for (int l : {60, 200, 400, 400, 400, 400, 200, 60}) s.hold(l, 300);
      s.hold(10, 3000);
    }
    expect("headlights sweeping past (2.4 s each) never fire", s.fired == 0);
  }
  {
    Sim s;
    s.hold(10, 3000);
    for (int flash = 0; flash < 3; flash++) {  // three lightning flashes ~1 s apart
      for (int stroke = 0; stroke < 4; stroke++) {
        s.hold(900, 50);  // one return stroke
        s.hold(10, 50);
      }
      s.hold(10, 700);
    }
    expect("lightning (50 ms strokes) never fires", s.fired == 0);
  }
  {
    Sim s;
    s.hold(500, 3000);
    s.covers(2, 500, 60, 400, 400);
    s.flashes(1, 500, 1023, 400, 400);
    expect("cover, cover, flash does not fire (mixed kinds)", s.fired == 0);
    s.flashes(2, 500, 1023, 400, 400);
    expect("...the flash started a new sequence: two more fire", s.fired == 1);
  }

  printf("light_gesture: %d failures\n", failures);
  return failures != 0;
}
