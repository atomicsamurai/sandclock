#pragma once

#include "time_ops.h"  // timeSynced
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

// True while the light sensor says it is dark and the user asked for quiet.
// Unknown light (no sample yet) never mutes. Manual beeps (/sound) ignore this.
bool buzzerMutedByDark() {
  return config.chimeMuteDark && lightLevel >= 0 && lightLevel < config.chimeDarkThreshold;
}

// Chime on the hour (double) and half hour (single). Called every second from
// loop(), independent of which screen is showing, so the chime still fires
// when the temperature screen covers the top of the minute.
void checkChime() {
  if (!timeSynced()) {
    return;  // unsynced RTC reads 1970; don't chime on a wrong hour
  }
  if (!config.chimeEnabled || buzzerMutedByDark()) {
    return;
  }
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
