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
