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

// fromSntp is false for a manual settimeofday() (the /forcetime debug button),
// which must not count as an NTP sync.
void onTimeSet(bool fromSntp) {
  if (!fromSntp) {
    return;
  }
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
// Reboot when the network and time have both been gone this long: safe mode
// cannot see a wedged-but-running clock, and a wall clock that is neither
// synced nor reachable has nothing to offer.
const unsigned long OFFLINE_REBOOT_MS = 30UL * 60 * 1000;
unsigned long offlineSinceMs = 0;

void rebootIfOffline() {
  if (WiFi.status() != WL_CONNECTED || !timeSynced()) {
    if (offlineSinceMs == 0) {
      offlineSinceMs = millis();
    } else if (millis() - offlineSinceMs >= OFFLINE_REBOOT_MS) {
      Log.errorln("[NTP] offline/unsynced for 30 minutes: rebooting to recover");
      ESP.restart();
    }
    return;
  }
  offlineSinceMs = 0;
}

void warnIfNtpStale() {
  if (timeSynced() || WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (millis() - lastNtpWarnMs >= 60000) {
    lastNtpWarnMs = millis();
    Log.warningln("[NTP] no time sync yet; check that the NTP servers are reachable");
  }
}
