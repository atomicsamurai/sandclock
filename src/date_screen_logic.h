#pragma once

// The date screen: "THU 19" in the 5x7 message font (scroll.h), centered.
// With dateMode once a minute it shows for DATE_SCREEN_SECS seconds in place
// of whatever the display style shows; with dateMode in the cycle,
// screen_cycle_logic.h decides when. Free of Arduino types so tests/ can compile it on a
// host; scroll.h draws it, tools/render.py and data/js/preview.js mirror it.

#include <stdio.h>

constexpr int DATE_SCREEN_FIRST_SEC = 30;  // mid-minute, away from the minute change
constexpr int DATE_SCREEN_SECS = 3;
constexpr int DATE_PANEL_WIDTH = 32;
constexpr int DATE_GLYPH_W = 5;     // font5x7 columns
constexpr int DATE_GAP = 1;         // between letters, as in a scrolling message
constexpr int DATE_SPACE_W = 3;     // the space between weekday and day (narrower than a glyph so two digits fit)

// Seconds of the minute (tm_sec, 0-60) during which the date is shown.
inline bool dateScreenDue(int sec) {
  return sec >= DATE_SCREEN_FIRST_SEC && sec < DATE_SCREEN_FIRST_SEC + DATE_SCREEN_SECS;
}

// Text width in columns: glyphs plus 1-column gaps, a space is DATE_SPACE_W.
inline int dateTextWidth(const char *s) {
  int w = 0;
  for (; *s; ++s) {
    w += (*s == ' ' ? DATE_SPACE_W : DATE_GLYPH_W + DATE_GAP);
  }
  return w > 0 ? w - DATE_GAP : 0;
}

// Fills out with "SUN 5" .. "SAT 31" for tm_wday (0 = Sunday) and tm_mday,
// and returns the x of its first column so it sits centered (an odd spare
// column goes on the right, as on the time screen).
inline int dateLayout(int wday, int mday, char out[8]) {
  static const char *const DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  if (wday < 0 || wday > 6) wday = 0;
  if (mday < 1 || mday > 31) mday = 1;
  snprintf(out, 8, "%s %d", DAYS[wday], mday);
  return (DATE_PANEL_WIDTH - dateTextWidth(out)) / 2;
}
