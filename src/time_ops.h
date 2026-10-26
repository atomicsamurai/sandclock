#pragma once
#include "timezones.h"

void applyTimezone() {
  Log.traceln("[applyTimezone]: TZ: %s", config.tzValue);
  configTime(config.tzValue, "pool.ntp.org");
  Log.traceln("[applyTimezone]: done");
}

/*
void updateUTCOffset() {
  Log.traceln("[UTCoffset]: here 1");
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  Log.traceln("[UTCoffset]: here 2");
  // client->setFingerprint(fingerprint);
  HTTPClient http; // Object of class HTTPClient
  if (http.begin(*client, "https://ipapi.co/json")) {
    Log.traceln("[UTCoffset]: here 3");
    int httpCode = http.GET();
    Log.traceln("[UTCoffset]: here 5");
    Log.traceln("[UTCoffset]: %d", httpCode);
    Log.traceln("[UTCoffset]: here 6");
    if (httpCode > 0) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, http.getString());

      const char *city = doc["city"];
      const char *utc_offset = doc["utc_offset"];
      Log.traceln("City: %s", city);
      Log.traceln("Offset: %s", utc_offset);
      char hour_offset[5];
      char minute_offset[5];
      hour_offset[0] = utc_offset[1];
      hour_offset[1] = utc_offset[2];
      hour_offset[2] = '\0';
      minute_offset[0] = utc_offset[3];
      minute_offset[1] = utc_offset[4];
      minute_offset[2] = '\0';

      timeOffset = (atol(hour_offset) * 3600) + (atol(minute_offset) * 60);
      if (utc_offset[0] == '-') {
        timeOffset = -timeOffset;
      }
      Log.traceln("Calculated offset: %l", timeOffset);
      // timeClient.setTimeOffset(timeOffset);
    }
    http.end(); // Close connection
  } else {
    Log.traceln("[UTCoffset]: here 4");
  }
}

void syncNTP(bool updateOffset) { // gets time from ntp and sets internal time accordingly, will return when no connection is established
  Log.traceln("Entering syncNTP()...");
  if (WiFi.status() != WL_CONNECTED) {
    Log.traceln("No active WiFi connection!");
    return;
  } // Sometimes the connection doesn't work right away although status is WL_CONNECTED...
  // if (updateOffset)
  //   updateUTCOffset(); // update the UTC offset if needed
  delay(500);          // ...so we'll wait a moment before causing network traffic
  // timeClient.update();
  // setTime(timeClient.getEpochTime());
  Log.traceln("device time: %l", now());
  // Log.traceln("ntp time : %l", timeClient.getEpochTime());
  Log.traceln("syncNTP() done...");
}
*/
