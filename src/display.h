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

  // Classic font digits are 5 wide; at hscale 2 that is 10 per digit.
  // The sign renders unscaled (5-col dash) so "-15" fits: 5+10+10+2+2 = 29
  // of 32 columns. Scaled, it would need 34 and clip the last digit.
  const auto &font = clock_font ("Classic");
  const int d14 = 11; // MINUS glyph index in the Classic font
  int y_start = 0;
  const int gap = 2;

  byte glyphs[4];
  int scale[4];
  int n = 0;
  if (currentTemp == -1000) { // no reading from HA
    for (int i = 0; i < 3; ++i) {
      glyphs[n] = d14;
      scale[n++] = 2;
    }
  } else {
    int positiveTemp = currentTemp < 0 ? -currentTemp : currentTemp;
    if (positiveTemp > 99)
      positiveTemp = 99;
    byte t1 = positiveTemp / 10;
    byte t2 = positiveTemp % 10;
    if (currentTemp < 0) {
      glyphs[n] = d14;
      scale[n++] = 1; // unscaled dash leaves room for two scaled digits
    }
    if (t1 > 0) {
      glyphs[n] = t1;
      scale[n++] = 2;
    }
    glyphs[n] = t2;
    scale[n++] = 2;
  }

  int total = 0;
  for (int i = 0; i < n; ++i) {
    total += font.glyphs[glyphs[i]].width * scale[i];
  }
  total += (n - 1) * gap;

  int x = (PanelWidth - total) / 2;
  for (int i = 0; i < n; ++i) {
    printAt (x, y_start, glyphs[i], font, config.temp_color, scale[i]);
    x += font.glyphs[glyphs[i]].width * scale[i] + gap;
  }
}

// The original small right-corner temperature (Temp font, 3x5 digits).
void showTemperatureSmall() {
  // keep the HA fetch cadence when the small style is the only renderer
  if (millis() - prevTimeTemperature >= 60000) {
    prevTimeTemperature = millis();
    currentTemp = getTemperatureFromHA();
  }

  const auto &font = clock_font ("Temp");
  int x_start = 22;
  int y_start = 1;

  if (currentTemp != -1000) {
    int positiveTemp = currentTemp < 0 ? -currentTemp : currentTemp;
    byte t1 = positiveTemp / 10;
    byte t2 = positiveTemp % 10;
    if (t1 == 0) {
      printAt (x_start, y_start, MINUS, font, currentTemp < 0 ? config.temp_color : color_black);
      printAt (x_start + 3, y_start, t2, font, config.temp_color);
    } else {
      printAt (x_start, y_start, MINUS, font, currentTemp < 0 ? config.temp_color : color_black);
      printAt (x_start + 3, y_start, t1, font, config.temp_color);
      printAt (x_start + 7, y_start, t2, font, config.temp_color);
    }
  } else {
    printAt (x_start, y_start, MINUS, font, config.temp_color);
    printAt (x_start + 3, y_start, MINUS, font, config.temp_color);
    printAt (x_start + 7, y_start, MINUS, font, config.temp_color);
  }
}

// Combined style (displayStyle 2): the original layout, time and temperature
// side by side every second. Time uses the Classic font at x=0..21, temp uses
// the small Temp font at x=22..31.
void showCombined() {
  showTime();
  showTemperatureSmall();
}
