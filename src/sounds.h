#pragma once

#include "vars.h"

void initSound() {
  pinMode(buzzerPin, OUTPUT);
}
void beep(int note, int duration){
  tone(buzzerPin, note, duration);
  delay(duration);
}

void singleBeep() {
  beep(587, 300);
}

void doubleBeep() {
  beep(659, 300);
  beep(587, 300);
}

// Chime on the hour (double) and half hour (single). Called every second from
// loop(), independent of which screen is showing, so the chime still fires
// when the temperature screen covers the top of the minute.
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
