#pragma once

#include "vars.h"

long getTemperatureFromHA() {
  if (strlen(config.hassAT) == 0 || strlen(config.hassBaseUrl) == 0 || strlen(config.hassEntityId) == 0) {
    Log.infoln("[HTTP] hass not configured, skipping reading temp...");
    return -1000;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return -1000;  // don't block the loop on a fetch that can't succeed
  }
  WiFiClient client;
  HTTPClient http;
  String hassBaseUrlString = config.hassBaseUrl;
  String hassEntityIdString = config.hassEntityId;
  String hassATString = config.hassAT;
  long temp = -1000;  // sentinel: caller treats -1000 as "no temperature"
  haLastAttemptMs = millis();
  Log.traceln("[HTTP] begin...");
  if (http.begin(client, hassBaseUrlString + "/api/states/" + hassEntityIdString)) {
    http.addHeader("Authorization", "Bearer " + hassATString);

    Log.traceln("[HTTP] GET...");
    // start connection and send HTTP header
    int httpCode = http.GET();
    haLastCode = httpCode > 0 ? httpCode : -1;

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Log.traceln("[HTTP] GET... code: %d", httpCode);

      if (httpCode == HTTP_CODE_OK) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, http.getString());
        const char *tempString = doc["state"];
        if (tempString != nullptr) {
          // "unavailable"/"unknown" must not parse as 0 degrees
          char *end;
          long v = strtol(tempString, &end, 10);
          if (end != tempString) {
            temp = v;
            haLastOkMs = haLastAttemptMs;
          } else {
            haLastCode = -2;
          }
        }
      }
    } else {
      Log.errorln("[HTTP] GET... failed, error: %s", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    haLastCode = -1;
    Log.errorln("[HTTP] Unable to connect");
  }
  return temp;
}
