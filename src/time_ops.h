#pragma once

#include <coredecls.h>  // settimeofday_cb
#include "vars.h"

/*
 * Timezone and NTP. The ESP8266 core's configTime() takes a POSIX TZ string
 * (config.tzValue) plus NTP servers; it keeps system time synced (hourly) and
 * applies the zone's DST rules automatically. Calling it again restarts SNTP,
 * which forces an immediate sync.
 */

// Before the first SNTP sync the RTC reads ~1970.
bool timeSynced() {
  return time(nullptr) > 1600000000;
}

void onTimeSet() {
  ntpLastSync = time(nullptr);
  ntpSyncCount++;
  Log.noticeln("[NTP] time set (update #%d)", (int)ntpSyncCount);
}

void applyTimezone() {
  Log.traceln("[applyTimezone]: TZ: %s", config.tzValue);
  settimeofday_cb(onTimeSet);
  configTime(config.tzValue, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
}

unsigned long lastNtpWarnMs = 0;

// Called about once a second; warns once a minute while online but unsynced.
void warnIfNtpStale() {
  if (timeSynced() || WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (millis() - lastNtpWarnMs >= 60000) {
    lastNtpWarnMs = millis();
    Log.warningln("[NTP] no time sync yet; check that the NTP servers are reachable");
  }
}
