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

  // Classic 5x7 font, edge-to-edge: slots H1@0-4, H2@7-11, colon@15,
  // M1@19-23, M2@27-31.
  const auto &font = clock_font ("Classic");
  int y_start = 0;

  if (h1 > 0) {
    // H1@0-4, H2@7-11, colon@15, M1@19-23, M2@27-31
    printAt (0, y_start, h1, font, config.clock_color);
    printAt (7, y_start, h2, font, config.clock_color);
    printAt (15, y_start, COLON, font, dotsOn ? config.clock_color : color_black);
    printAt (19, y_start, m1, font, config.clock_color);
    printAt (27, y_start, m2, font, config.clock_color);
  } else {
    // single-digit hour: center the four remaining glyphs
    printAt (3, y_start, h2, font, config.clock_color);
    printAt (12, y_start, COLON, font, dotsOn ? config.clock_color : color_black);
    printAt (17, y_start, m1, font, config.clock_color);
    printAt (25, y_start, m2, font, config.clock_color);
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
