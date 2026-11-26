#pragma once

// Which screen the clock shows, second by second, and when the date appears.
// The date setting (dateMode) is off, once a minute (3 s at :30, over
// whatever else is showing), or a step in the cycle:
//   cycle style:    time 5 s -> temperature 2 s [-> date 2 s] -> time ...
//   combined style: combined 5 s [-> date 2 s] -> combined ...
// Without a temperature reading the temperature screen is left out (the
// combined style then shows the time screen as its main screen; see loop()).
// Free of Arduino types so tests/ can compile it on a host; data/js/preview.js
// mirrors the order for the web UI preview.

#include <stdint.h>

constexpr uint8_t DATE_MODE_OFF = 0;
constexpr uint8_t DATE_MODE_MINUTE = 1;  // 3 s once a minute (date_screen_logic.h)
constexpr uint8_t DATE_MODE_CYCLE = 2;   // a step in the screen cycle

constexpr uint8_t SCREEN_MAIN = 0;  // the time screen, or the combined screen
constexpr uint8_t SCREEN_TEMP = 1;  // cycle style only
constexpr uint8_t SCREEN_DATE = 2;  // dateMode DATE_MODE_CYCLE only

constexpr unsigned long MAIN_SCREEN_MS = 5000;
constexpr unsigned long TEMP_SCREEN_MS = 2000;
constexpr unsigned long DATE_CYCLE_SCREEN_MS = 2000;

// The stored setting from a settings document: dateMode when present
// (anything out of range means once a minute), else the older dateEnabled
// true/false, else the default, once a minute. hasMode/hasEnabled say which
// keys the document has.
inline uint8_t dateModeFrom(bool hasMode, long mode, bool hasEnabled, bool enabled) {
  if (hasMode) {
    return (mode >= DATE_MODE_OFF && mode <= DATE_MODE_CYCLE) ? (uint8_t)mode : DATE_MODE_MINUTE;
  }
  if (hasEnabled) {
    return enabled ? DATE_MODE_MINUTE : DATE_MODE_OFF;
  }
  return DATE_MODE_MINUTE;
}

inline unsigned long screenDurationMs(uint8_t screen) {
  return screen == SCREEN_MAIN ? MAIN_SCREEN_MS : screen == SCREEN_TEMP ? TEMP_SCREEN_MS : DATE_CYCLE_SCREEN_MS;
}

// Whether screen is part of the cycle for this style (1 = cycle,
// 2 = combined), date mode and whether there is a temperature reading.
inline bool screenInCycle(uint8_t screen, uint8_t style, uint8_t dateMode, bool hasTemp) {
  if (screen == SCREEN_MAIN) return true;
  if (screen == SCREEN_TEMP) return style != 2 && hasTemp;
  if (screen == SCREEN_DATE) return dateMode == DATE_MODE_CYCLE;
  return false;
}

// The screen after this one.
inline uint8_t nextScreen(uint8_t screen, uint8_t style, uint8_t dateMode, bool hasTemp) {
  for (uint8_t s = screen + 1; s <= SCREEN_DATE; s++) {
    if (screenInCycle(s, style, dateMode, hasTemp)) return s;
  }
  return SCREEN_MAIN;
}

struct ScreenCycle {
  uint8_t screen = SCREEN_MAIN;
  unsigned long startMs = 0;

  // Called once a second; moves on when the current screen's time is up.
  // A screen the cycle no longer includes (style or date mode changed, or
  // the temperature reading was lost, while it showed) gives way to the
  // main screen at once.
  uint8_t step(unsigned long nowMs, uint8_t style, uint8_t dateMode, bool hasTemp) {
    if (!screenInCycle(screen, style, dateMode, hasTemp)) {
      screen = SCREEN_MAIN;
      startMs = nowMs;
    } else if (nowMs - startMs >= screenDurationMs(screen)) {
      screen = nextScreen(screen, style, dateMode, hasTemp);
      startMs = nowMs;
    }
    return screen;
  }
};
