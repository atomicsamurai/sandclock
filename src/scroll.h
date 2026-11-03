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
 * Uses getCharIndex and the font5x7 table defined below.
 */

const uint8_t font5x7[][5] PROGMEM = {
    {0x7E, 0x09, 0x09, 0x7E, 0x00},  // A
    {0x7F, 0x49, 0x49, 0x36, 0x00},  // B
    {0x3E, 0x41, 0x41, 0x22, 0x00},  // C
    {0x7F, 0x41, 0x41, 0x3E, 0x00},  // D
    {0x7F, 0x49, 0x49, 0x41, 0x00},  // E
    {0x7F, 0x09, 0x09, 0x01, 0x00},  // F
    {0x3E, 0x41, 0x51, 0x32, 0x00},  // G
    {0x7F, 0x08, 0x08, 0x7F, 0x00},  // H
    {0x41, 0x7F, 0x41, 0x00, 0x00},  // I
    {0x20, 0x40, 0x41, 0x3F, 0x00},  // J
    {0x7F, 0x08, 0x14, 0x63, 0x00},  // K
    {0x7F, 0x40, 0x40, 0x40, 0x40},  // L
    {0x7F, 0x02, 0x04, 0x02, 0x7F},  // M
    {0x7F, 0x06, 0x18, 0x7F, 0x00},  // N
    {0x3E, 0x41, 0x41, 0x3E, 0x00},  // O
    {0x7F, 0x09, 0x09, 0x06, 0x00},  // P
    {0x3E, 0x41, 0x61, 0x7E, 0x00},  // Q
    {0x7F, 0x09, 0x19, 0x66, 0x00},  // R
    {0x46, 0x49, 0x49, 0x31, 0x00},  // S
    {0x01, 0x7F, 0x01, 0x00, 0x00},  // T
    {0x3F, 0x40, 0x40, 0x3F, 0x00},  // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F},  // V
    {0x7F, 0x20, 0x18, 0x20, 0x7F},  // W
    {0x63, 0x14, 0x08, 0x14, 0x63},  // X
    {0x07, 0x08, 0x70, 0x08, 0x07},  // Y
    {0x61, 0x51, 0x49, 0x45, 0x43},  // Z
    {0x3E, 0x51, 0x49, 0x45, 0x3E},  // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00},  // 1
    {0x42, 0x61, 0x51, 0x49, 0x46},  // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31},  // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10},  // 4
    {0x27, 0x45, 0x45, 0x45, 0x39},  // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30},  // 6
    {0x01, 0x71, 0x09, 0x05, 0x03},  // 7
    {0x36, 0x49, 0x49, 0x49, 0x36},  // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E},  // 9
    {0x08, 0x08, 0x08, 0x08, 0x08},  // -
    {0x00, 0x24, 0x00, 0x00, 0x00}   // :
};


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
bool scrollSilent = false;  // the running scroll was started with scrollQuiet

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
          strip.SetPixelColor(pixelIndex, config.other_color);
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
      if (!scrollSilent && !buzzerMutedByDark()) {
        doubleBeep();  // the base's end-of-scroll signal
      }
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
  scrollSilent = scrollQuiet;
  scrollQuiet = false;
  if (!scrollSilent && !buzzerMutedByDark()) {
    doubleBeep();  // the base's start-of-scroll signal
  }
}
