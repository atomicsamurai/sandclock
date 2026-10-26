#pragma once

#include "vars.h"

/*
 * Timezone handling. The ESP8266 core's configTime() takes a POSIX TZ string
 * (config.tzValue) plus an NTP server; it keeps system time synced and applies
 * the zone's DST rules automatically.
 */

void applyTimezone() {
  Log.traceln("[applyTimezone]: TZ: %s", config.tzValue);
  configTime(config.tzValue, "pool.ntp.org");
  Log.traceln("[applyTimezone]: done");
}
