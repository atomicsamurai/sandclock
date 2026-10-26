#pragma once

#include "vars.h"

/*
 * Non-blocking scrolling text for the matrix. An external app triggers it
 * via POST /showmessage (see api.h): the handler fills messageText /
 * messageScrollDelayMs / messageRepeatCount and sets showingMessage, and
 * loop() starts the scroll. tickScroll() advances one column per step from
 * loop(), so the web server, chime and brightness stay live while text
 * scrolls. When the scroll ends the clock redraws immediately.
 *
 * Uses font5x7 and getCharIndex from digits.h.
 */

int getCharIndex(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';  // A-Z maps to 0-25
  } else if (c >= 'a' && c <= 'z') {
    return c - 'a';  // a-z renders as uppercase
  } else if (c >= '0' && c <= '9') {
    return c - '0' + 26;  // 0-9 maps to 26-35
  } else if (c == '-') {
    return 36;  // dash maps to 36
  } else if (c == ':') {
    return 37;  // colon maps to 37
  }
  return -1;  // characters without a glyph are skipped (still occupy width)
}

bool scrollActive = false;
RgbColor scrollColor (255, 60, 0);  // the base's scroll color

void renderScrollFrame(int pos) {
  strip.ClearTo(color_black);
  int totalLen = strlen(messageText);
  for (int charIdx = 0; charIdx < totalLen; charIdx++) {
    char c = messageText[charIdx];
    if (c == ' ') continue;
    int charIndex = getCharIndex(c);
    if (charIndex < 0) continue;

    int charStartX = pos + charIdx * 6;
    if (charStartX >= PanelWidth || charStartX + 5 < 0) continue;

    for (uint8_t col = 0; col < 5; col++) {
      int pixelX = charStartX + col;
      if (pixelX < 0 || pixelX >= PanelWidth) continue;
      uint8_t columnData = pgm_read_byte(&(font5x7[charIndex][col]));
      for (uint8_t row = 0; row < 7; row++) {
        if (columnData & (1 << row)) {
          uint16_t pixelIndex = topo.Map(pixelX, row);
          strip.SetPixelColor(pixelIndex, scrollColor);
        }
      }
    }
  }
  strip.Show();
}

int scrollPos = 0;
int scrollTotalWidth = 0;
uint16_t passesLeft = 0;
unsigned long scrollLastStep = 0;

void tickScroll() {
  if (!scrollActive) {
    return;
  }
  unsigned long now = millis();
  if (scrollLastStep != 0 && now - scrollLastStep < messageScrollDelayMs) {
    return;
  }
  scrollLastStep = now;

  renderScrollFrame(scrollPos);
  scrollPos--;
  if (scrollPos < -scrollTotalWidth) {  // one full pass done
    passesLeft--;
    if (passesLeft == 0) {
      scrollActive = false;
      strip.ClearTo(color_black);
      strip.Show();
      prevTime = 0;  // redraw the clock immediately instead of waiting a second
      doubleBeep();  // the base's end-of-scroll signal
      return;
    }
    scrollPos = PanelWidth;  // start the next pass
  }
}

// Called from loop() when showingMessage is set by /showmessage.
void beginMessageScroll() {
  scrollTotalWidth = strlen(messageText) * 6 - 1;  // 5px glyph + 1px spacing
  scrollPos = PanelWidth;                          // enter from the right edge
  passesLeft = messageRepeatCount > 0 ? messageRepeatCount : 1;
  scrollLastStep = 0;                              // render first frame now
  scrollActive = true;
  doubleBeep();  // the base's start-of-scroll signal
}
