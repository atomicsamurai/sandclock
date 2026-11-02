#pragma once

#include <ArduinoJson.h>
#include <stdlib.h>

// Extracts the integer temperature from Home Assistant's /api/states/<entity>
// reply. Only "state" is kept, so a sensor with a large attribute list can't
// overflow the document. Anything that isn't a number ("unavailable",
// "unknown", missing, invalid JSON) returns false and leaves temp untouched.
// Header-only so tests/ can compile it on a host.
template <typename Payload>
bool parseHaTemperature(const Payload &payload, long &temp) {
  StaticJsonDocument<64> filter;
  filter["state"] = true;
  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  const char *s = err ? nullptr : doc["state"].as<const char *>();
  if (!s) {
    return false;
  }
  char *end = nullptr;
  long v = strtol(s, &end, 10);
  if (end == s) {
    return false;
  }
  temp = v;
  return true;
}
