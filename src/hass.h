#pragma once

#include "ha_parse.h"
#include "vars.h"

bool hassConfigured() {
  return config.hassAT[0] && config.hassBaseUrl[0] && config.hassEntityId[0];
}

long getTemperatureFromHA() {
  if (!hassConfigured()) {
    Log.infoln("[HTTP] hass not configured, skipping reading temp...");
    return -1000;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return -1000;  // don't block the loop on a fetch that can't succeed
  }
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(2000);  // blocks loop(): keep an unreachable HA from stalling the clock for long
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
        if (parseHaTemperature(http.getString(), temp)) {
          haLastOkMs = haLastAttemptMs;
        } else {
          haLastCode = -2;
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

const unsigned long TEMP_FETCH_MS = 60000;           // how often to ask HA
const unsigned long TEMP_STALE_MS = 15UL * 60000;    // show "---" once the last good reading is this old

// Called once a second from loop(). Fetches right after WiFi comes up, then
// every TEMP_FETCH_MS, whichever screen is showing. A failed fetch keeps the
// last good reading for TEMP_STALE_MS so a brief HA outage doesn't blank it.
void refreshTemperatureIfDue() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (tempFetchTried && millis() - prevTimeTemperature < TEMP_FETCH_MS) {
    return;
  }
  tempFetchTried = true;
  prevTimeTemperature = millis();
  long t = getTemperatureFromHA();
  if (t != -1000) {
    currentTemp = t;
  } else if (!hassConfigured() || haLastOkMs == 0 || millis() - haLastOkMs > TEMP_STALE_MS) {
    currentTemp = -1000;
  }
}
