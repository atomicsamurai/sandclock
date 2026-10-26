#pragma once

long getTemperatureFromHA() {
  WiFiClient client;
  HTTPClient http;
  if (strlen(config.hassAT) == 0 || strlen(config.hassBaseUrl) == 0 || strlen(config.hassEntityId) == 0) {
    Log.infoln("[HTTP] hass not configured, skipping reading temp...");
    return -1000;
  }
  String hassBaseUrlString = config.hassBaseUrl;
  String hassEntityIdString = config.hassEntityId;
  String hassATString = config.hassAT;
  long temp = -1000;  // sentinel: caller treats -1000 as "no temperature"
  Log.traceln("[HTTP] begin...");
  // if (http.begin(client, "http://sandhass.sand.lan:8123/api/states/sensor.deck_temp")) {
  if (http.begin(client, hassBaseUrlString + "/api/states/" + hassEntityIdString)) {
    // if (http.begin(client, url)) {
    // http.addHeader("Authorization", "Bearer YOUR_HA_LONG_LIVED_TOKEN");
    http.addHeader("Authorization", "Bearer " + hassATString);

    Log.traceln("[HTTP] GET...");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Log.traceln("[HTTP] GET... code: %d", httpCode);

      if (httpCode == HTTP_CODE_OK) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, http.getString());
        const char *tempString = doc["state"];
        if (tempString != nullptr) {
          temp = atol(tempString);
        }
      }
    } else {
      Log.errorln("[HTTP] GET... failed, error: %s", http.errorToString(httpCode).c_str());
      temp = -1000;
    }
    http.end();
  } else {
    Log.errorln("[HTTP] Unable to connect");
    temp = -1000;
  }
  return temp;
}
