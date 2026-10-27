#pragma once

#include "digits.h"
#include "hass.h"  // getTemperatureFromHA, used by showTemperature()

void clearDisplay() {
  for (int i = 0; i != PanelWidth; ++i) {
    for (int j = 0; j != PanelHeight; ++j) {
      auto pixel = topo.Map (i, j);
      strip.SetPixelColor(pixel, color_black);
    }
  }
}

// Draw one glyph with its left edge at x. hscale repeats each glyph column
// so a glyph can be widened without new font data (temp screen uses hscale 2).
void printAt(int x, int y, int digit, const Font &font, RgbColor color, int hscale = 1) {
  const auto &glyph = font.glyphs[digit];
  for (int i = 0; i < glyph.width; ++i) {
    for (int j = 0; j < font.height; ++j) {
      if (glyph.data[j] & (1 << ((glyph.width - 1) - i))) {
        for (int s = 0; s < hscale; ++s) {
          int px = x + i * hscale + s;
          int py = y + j;
          if (px >= 0 && px < PanelWidth && py >= 0 && py < PanelHeight) {
            auto pixel = topo.Map (px, py);
            strip.SetPixelColor(pixel, color);
          }
        }
      }
    }
  }
}

void showTime() {
  auto now = ::time (nullptr);
  auto tm = ::localtime (&now);
  int thishour = tm->tm_hour;
  int thismins = tm->tm_min;
  Log.verboseln("time: %d:%d:%d", thishour, thismins, tm->tm_sec);

  if (config.hourFormat == 12) {
    if (thishour == 0)
      thishour = 12;
    else if (thishour > 12)
      thishour = thishour - 12;
  }

  byte h1 = thishour / 10;
  byte h2 = thishour % 10;
  byte m1 = thismins / 10;
  byte m2 = thismins % 10;

  // UserBold font: the hand-drawn design emboldened to 2px strokes, all 8
  // rows. Slots H1@1-6, H2@8-13, colon@15-16, M1@18-23, M2@25-30; 1-col
  // gaps, 1-col margins.
  const auto &font = clock_font ("UserBold");
  int y_start = 0;

  if (h1 > 0) {
    // H1@1-6, H2@8-13, colon@15-16, M1@18-23, M2@25-30
    printAt (1, y_start, h1, font, config.clock_color);
    printAt (8, y_start, h2, font, config.clock_color);
    printAt (15, y_start, COLON, font, dotsOn ? config.clock_color : color_black);
    printAt (18, y_start, m1, font, config.clock_color);
    printAt (25, y_start, m2, font, config.clock_color);
  } else {
    // single-digit hour: center the four glyphs (4*6+3*1 = 27, margins 2/3)
    printAt (2, y_start, h2, font, config.clock_color);
    printAt (10, y_start, COLON, font, dotsOn ? config.clock_color : color_black);
    printAt (14, y_start, m1, font, config.clock_color);
    printAt (21, y_start, m2, font, config.clock_color);
  }
  dotsOn = !dotsOn;
}

void showTemperature() {
  if (millis() - prevTimeTemperature >= 60000) { // refresh from HA every minute
    prevTimeTemperature = millis();
    currentTemp = getTemperatureFromHA();
  }

  // Exact same UserBold glyphs at scale 1 as the clock, laid out like
  // showTime(): 6-wide glyphs, 2-wide colon slots, 1-col gaps, 1-col
  // margins (slots 1/8/15/18/25). "-15" renders as MINUS + "1" + "5" in
  // the first three slots; unused trailing slots stay dark.
  const auto &font = clock_font ("UserBold");
  const int dMinus = 11; // MINUS glyph index in the UserBold font
  int y_start = 0;

  byte glyphs[4];  // up to 3 digits/sign + degree glyph
  int n = 0;
  int slot[4];
  int degreeSlot = -1;
  if (currentTemp == -1000) { // no reading from HA: "---" in slots 1-3
    slot[0] = 1; slot[1] = 8; slot[2] = 18;
    for (int i = 0; i < 3; ++i) {
      glyphs[n++] = dMinus;
    }
  } else {
    int positiveTemp = currentTemp < 0 ? -currentTemp : currentTemp;
    if (positiveTemp > 99)
      positiveTemp = 99;
    byte t1 = positiveTemp / 10;
    byte t2 = positiveTemp % 10;
    if (currentTemp < 0) {
      slot[0] = 1; slot[1] = 8; slot[2] = 18;
      glyphs[n++] = dMinus;
      if (t1 > 0) {
        glyphs[n++] = t1;
      }
      glyphs[n++] = t2;
      degreeSlot = slot[n-1] + 7;
    } else if (t1 > 0) {
      slot[0] = 1; slot[1] = 8; slot[2] = 15;
      glyphs[n++] = t1;
      glyphs[n++] = t2;
      degreeSlot = 22;
    } else {
      slot[0] = 1; slot[1] = 8;
      glyphs[n++] = t1;
      glyphs[n++] = t2;
      degreeSlot = 15;
    }
  }

  for (int i = 0; i < n; ++i) {
    printAt (slot[i], y_start, glyphs[i], font, config.temp_color);
  }
  if (degreeSlot >= 0) {
    printAt (degreeSlot, y_start, DEGREES, font, config.temp_color);
  }
}


// The original small right-corner temperature (Temp font, 3x5 digits).
// In the combined view it owns the fixed region x=22..31.
void showTemperatureSmall() {
  // keep the HA fetch cadence when the small style is the only renderer
  if (millis() - prevTimeTemperature >= 60000) {
    prevTimeTemperature = millis();
    currentTemp = getTemperatureFromHA();
  }

  const auto &font = clock_font ("Temp");
  const int x_start = 22;
  int y_start = 1;

  if (currentTemp != -1000) {
    int positiveTemp = currentTemp < 0 ? -currentTemp : currentTemp;
    if (positiveTemp > 99)
      positiveTemp = 99;
    byte t1 = positiveTemp / 10;
    byte t2 = positiveTemp % 10;
    if (currentTemp < 0) {
      // sign@22-23, then digits at 25 and 29: "-15" spans 22..31 exactly
      printAt (x_start, y_start, MINUS, font, config.temp_color);
      if (t1 > 0) {
        printAt (x_start + 3, y_start, t1, font, config.temp_color);
        printAt (x_start + 7, y_start, t2, font, config.temp_color);
      } else {
        printAt (x_start + 3, y_start, t2, font, config.temp_color);
        printAt (x_start + 8, y_start, DEGREES, font, config.temp_color);
      }
    } else if (t1 > 0) {
      // "21" at 22..28
      printAt (x_start, y_start, t1, font, config.temp_color);
      printAt (x_start + 4, y_start, t2, font, config.temp_color);
      printAt (x_start + 8, y_start, DEGREES, font, config.temp_color);
    } else {
      // "7" at 22..24
      printAt (x_start, y_start, t2, font, config.temp_color);
      printAt (x_start + 4, y_start, DEGREES, font, config.temp_color);
    }
  } else {
    // "---" across the region
    printAt (x_start, y_start, MINUS, font, config.temp_color);
    printAt (x_start + 3, y_start, MINUS, font, config.temp_color);
    printAt (x_start + 7, y_start, MINUS, font, config.temp_color);
  }
}

// Combined style (displayStyle 2): fixed split, time left, temp right.
// Time: Compact font (4x8) in x=0..21, slots H1@0 H2@5 colon@10 M1@13 M2@18.
// Temp: Temp font (3x5) in x=22..31, top-aligned at y=1.
void showCombined() {
  auto now = ::time (nullptr);
  auto tm = ::localtime (&now);
  int thishour = tm->tm_hour;
  int thismins = tm->tm_min;

  if (config.hourFormat == 12) {
    if (thishour == 0)
      thishour = 12;
    else if (thishour > 12)
      thishour = thishour - 12;
  }
  byte h1 = thishour / 10;
  byte h2 = thishour % 10;
  byte m1 = thismins / 10;
  byte m2 = thismins % 10;

  const auto &font = clock_font ("Compact");
  const int y_start = 0;

  if (h1 > 0) {
    // H1@0-3, H2@5-8, colon@10-11, M1@13-16, M2@18-21
    printAt (0, y_start, h1, font, config.clock_color);
    printAt (5, y_start, h2, font, config.clock_color);
    printAt (10, y_start, COLON, font, dotsOn ? config.clock_color : color_black);
    printAt (13, y_start, m1, font, config.clock_color);
    printAt (18, y_start, m2, font, config.clock_color);
  } else {
    // single-digit hour: same right-anchored slots, H at 5
    printAt (5, y_start, h2, font, config.clock_color);
    printAt (10, y_start, COLON, font, dotsOn ? config.clock_color : color_black);
    printAt (13, y_start, m1, font, config.clock_color);
    printAt (18, y_start, m2, font, config.clock_color);
  }
  dotsOn = !dotsOn;

  showTemperatureSmall();
}
