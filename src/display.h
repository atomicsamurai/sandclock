#pragma once

#include "digits.h"
#include "sounds.h"

// void warningBlinkInfinite() {
//   byte count = 0;
//   while (true) {
//     if (count >= 60) {
//       count = 0;
//     }
//     delay(100);
    
//     LEDs[count] = CRGB::Red;
//     FastLED.show();
//     count++;
//   }
// }

void clearDisplay() {
  for (int i = 0; i != PanelWidth; ++i) {
    for (int j = 0; j != PanelHeight; ++j) {
      auto pixel = topo.Map (i, j);
      strip.SetPixelColor(pixel, color_black);
    }
  }
}

void print(int &x, int &y, int digit, const Font &font, RgbColor color) {
  const auto &glyph = font.glyphs[digit];
  for (int i = 0; i < glyph.width; ++i) {
    for (int j = 0; j < font.height; ++j) {
      auto ix = x + i;
      auto iy = y + j;
      if (ix >= 0 && ix < PanelWidth && iy >= 0 && iy < PanelHeight) {
        if (glyph.data[j] & (1 << ((glyph.width - 1) - i))) {
          auto addsat = [] (int a, int b) { return a + b > 255 ? 255 : a + b; };
          auto pixel = topo.Map (ix, iy);
          strip.SetPixelColor(pixel, color);
        }
      } 
    }
  }
  x += glyph.width + 1;
}

// Chime on the hour (double) and half hour (single). Called every second from
// loop(), independent of what is on screen, so a scroll covering the top of
// the minute no longer swallows the chime.
void checkChime() {
  auto now = ::time (nullptr);
  auto tm = ::localtime (&now);
  if (tm->tm_sec == 0 && beepDone == 0 && (tm->tm_min == 0 || tm->tm_min == 30)) {
    if (tm->tm_min == 30) {
      singleBeep();
    } else {
      doubleBeep();
    }
    beepDone = 1;
  } else {
    beepDone = 0;
  }
}

void showTime() {
  auto now = ::time (nullptr);
  auto tm = ::localtime (&now);
  int thishour = tm->tm_hour;
  int thismins = tm->tm_min;

  int x_start = 0;
  int y_start = 1;
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
  const auto &font = clock_font ("Time");

  if (h1 > 0) {
    print (x_start, y_start, h1, font, config.clock_color);
  } else {
    print (x_start, y_start, BLANK, font, config.clock_color);
  }
  print (x_start, y_start, h2, font, config.clock_color);

  if (dotsOn) {
    print (x_start, y_start, COLON, font, config.clock_color);
  } else {
    print (x_start, y_start, COLON, font, color_black);
  }
  dotsOn = !dotsOn;
  
  print (x_start, y_start, m1, font, config.clock_color);
  print (x_start, y_start, m2, font, config.clock_color);

}

void showTemperature() {
  tempFetchCounter++;
  if (tempFetchCounter > 100) {
    currentTemp = getTemperatureFromHA();
    tempFetchCounter = 0;
  }
 
  const auto &font = clock_font ("Temp");
  int x_start = 22;
  int y_start = 1;

  // long temp = getTemperatureFromHA();
  Log.traceln("temp: %d", currentTemp);
  if (currentTemp != -1000) {
    // temp fetched successfully
    int positiveTemp = currentTemp;
    if (currentTemp < 0)
      positiveTemp = -currentTemp;
    byte t1 = int(positiveTemp) / 10;
    byte t2 = int(positiveTemp) % 10;
    Log.traceln("first digit: %d", t2);
    
    Log.traceln("second digit: %d", t1);
    if (t1 == 0) {
      if (currentTemp < 0) {
        Log.traceln("third digit: -");
        // displayNumber(negativeSymbol, 1, color);
        print (x_start, y_start, MINUS, font, config.temp_color);
      } else {
        Log.traceln("third digit: ");
        // displayNumber(offDigit, 1, color);
        print (x_start, y_start, MINUS, font, color_black);
      }
      // displayNumber(t2, 0, color);
      print (x_start, y_start, t2, font, config.temp_color);
      print (x_start, y_start, t2, font, color_black);
    } else {
      if (currentTemp < 0) {
        Log.traceln("third digit: -");
        // displayNumber(negativeSymbol, 2, color);
        print (x_start, y_start, MINUS, font, config.temp_color);
      } else {
        print (x_start, y_start, MINUS, font, color_black);
      }
      print (x_start, y_start, t1, font, config.temp_color);
      print (x_start, y_start, t2, font, config.temp_color);
    }
  } else {
    print (x_start, y_start, MINUS, font, config.temp_color);
    print (x_start, y_start, MINUS, font, config.temp_color);
    print (x_start, y_start, MINUS, font, config.temp_color);
  }
}

void handleBrightness() {
  if (config.autoBrightness) {
    sensorValue = analogRead(sensorPin);
    // sensorValue varies from 0 to 1023
    // brightness should be from 15 (min) to 255
    config.brightness = (sensorValue * 215 / 1023) + 15;
    Log.traceln("HB - sensor val: %d, voltage: %F, brightness calculated: %d", sensorValue, voltage, config.brightness);
  }
  // Log.traceln("setting brightness: %d", brightness);
  strip.SetBrightness(config.brightness);
}
