#pragma once

#include "vars.h"

/*
 * Ambient-light-driven brightness. handleBrightness() runs on the 5s tick
 * from loop() and maps the LDR reading on A0 to the panel brightness.
 */

void handleBrightness() {
  // Always sampled: the buzzer's dark-mute needs it even with manual brightness.
  sensorValue = analogRead(sensorPin);
  lightLevel = lightLevel < 0 ? sensorValue : (lightLevel * 3 + sensorValue) / 4;
  if (config.autoBrightness) {
    // sensorValue varies from 0 to 1023
    // brightness should be from 15 (min) to 255
    config.brightness = (sensorValue * 215 / 1023) + 15;
    Log.traceln("HB - sensor val: %d, brightness calculated: %d", sensorValue, config.brightness);
  }
  strip.SetBrightness(config.brightness);
}
