#pragma once

#include "config.h"
#include "hass.h"
#include "safe_mode.h"
#include "scroll.h"
#include "sounds.h"
#include "time_ops.h"
#include "vars.h"

/*
 * Optional HTTP Basic auth (user "admin"). With no password set every
 * endpoint stays open, as before; once set, every API call, the OTA page and
 * the index page need it. Basic auth sends the password in clear text, so
 * this keeps casual and accidental access out, not an attacker on the LAN.
 */
const char *const AUTH_USER = "admin";

bool authorized() {
  if (config.authPassword[0] == 0 || server.authenticate(AUTH_USER, config.authPassword)) {
    return true;
  }
  server.requestAuthentication(BASIC_AUTH, "SandClock");
  return false;
}

ESP8266WebServer::THandlerFunction secured(ESP8266WebServer::THandlerFunction handler) {
  return [handler]() {
    if (authorized()) {
      handler();
    }
  };
}

void serveIndex() {
  File f = LittleFS.open("/index.html", "r");
  if (!f) {
    server.send(404, "text/plain", "404: index.html missing, flash the filesystem image");
    return;
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.streamFile(f, "text/html");
  f.close();
}

void getConfig() {
  char buffer[1024];
  serializeJson(getConfigDoc(true), buffer);
  Log.verboseln("get config: %s", buffer);
  server.send(200, "text/json", buffer);
}

void updateWifiConfig() {
  if (server.arg("wifi_ssid") != "") {
    strlcpy(config.wifi_ssid, server.arg("wifi_ssid").c_str(), sizeof(config.wifi_ssid));
  }
  if (server.arg("wifi_password") != "" && server.arg("wifi_password") != SECRET_MASK) {
    strlcpy(config.wifi_password, server.arg("wifi_password").c_str(), sizeof(config.wifi_password));
  }
  Log.traceln("update wifiConfig");
  saveConfig();
  // A clean reboot applies the new credentials: at boot the station is tried
  // before the setup AP comes up, with no client pause in the way.
  rebootPending = true;
  rebootAtMs = millis() + 1500;
  getConfig();
}

void updateHassConfig() {
  if (server.arg("hassBaseUrl") != "") {
    strlcpy(config.hassBaseUrl, server.arg("hassBaseUrl").c_str(), sizeof(config.hassBaseUrl));
  }
  if (server.arg("hassEntityId") != "") {
    strlcpy(config.hassEntityId, server.arg("hassEntityId").c_str(), sizeof(config.hassEntityId));
  }
  if (server.arg("hassAT") != "" && server.arg("hassAT") != SECRET_MASK) {
    strlcpy(config.hassAT, server.arg("hassAT").c_str(), sizeof(config.hassAT));
  }
  Log.traceln("set hassConfig");
  saveConfig();
  // sometehing changed, get latest temp from hass
  currentTemp = getTemperatureFromHA();
  tempFetchTried = true;
  prevTimeTemperature = millis();
  getConfig();
}

RgbColor argColor() {
  // toInt wraps out-of-range values; mask to a channel's valid range
  return RgbColor(server.arg("r").toInt() & 0xFF,
                  server.arg("g").toInt() & 0xFF,
                  server.arg("b").toInt() & 0xFF);
}

void setClockColor() {
  config.clock_color = argColor();
  Log.traceln("set clock color");
  saveConfig();
  getConfig();
}

void setTempColor() {
  config.temp_color = argColor();
  Log.traceln("set temp color");
  saveConfig();
  getConfig();
}

void setOtherColor() {
  config.other_color = argColor();
  Log.traceln("set other color");
  saveConfig();
  getConfig();
}

void playSound() {
  if(!server.hasArg("type")) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  Log.traceln("play sound: %s", server.arg("type").c_str());
  if(server.arg("type") == "single") {
    singleBeep();
  } else {
    doubleBeep();
  }
  server.send(200, "text/json", "{\"result\":\"ok\"}");
}

void setBrightness() {
  if(!server.hasArg("brightness")) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  long b = server.arg("brightness").toInt();
  if (b < 0 || b > 255) {
    server.send(400, "text/plain", "400: brightness must be 0-255");
    return;
  }
  config.brightness = b;
  Log.traceln("set brightness");
  saveConfig();
  getConfig();
}

void toggleAutoBrightness() {
  if(!server.hasArg("autoBrightness")) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  if(server.arg("autoBrightness") == "true") {
    config.autoBrightness = true;
    Log.noticeln("enable auto brightness");
  } else {
    config.autoBrightness = false;
    Log.traceln("disable auto brightness");
  }
  saveConfig();
  getConfig();
}

void setHourFormat() {
  if(!server.hasArg("hourFormat")) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  long f = server.arg("hourFormat").toInt();
  if (f != 12 && f != 24) {
    server.send(400, "text/plain", "400: hourFormat must be 12 or 24");
    return;
  }
  config.hourFormat = f;
  Log.traceln("set hour format");
  saveConfig();
  getConfig();
}

void setDisplayStyle() {
  if (!server.hasArg("displayStyle") && !server.hasArg("dateEnabled")) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  if (server.hasArg("displayStyle")) {
    long s = server.arg("displayStyle").toInt();
    if (s != 1 && s != 2) {
      server.send(400, "text/plain", "400: displayStyle must be 1 (cycle) or 2 (combined)");
      return;
    }
    config.displayStyle = s;
    Log.traceln("set display style: %d", s);
  }
  if (server.hasArg("dateEnabled")) {
    config.dateEnabled = server.arg("dateEnabled") == "true";
  }
  saveConfig();
  getConfig();
}

void setChime() {
  bool any = server.hasArg("chimeEnabled") || server.hasArg("chimeMuteDark") || server.hasArg("chimeDarkThreshold");
  if (!any) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  if (server.hasArg("chimeDarkThreshold")) {
    long t = server.arg("chimeDarkThreshold").toInt();
    if (t < 0 || t > 1023) {
      server.send(400, "text/plain", "400: chimeDarkThreshold must be 0-1023");
      return;
    }
    config.chimeDarkThreshold = t;
  }
  if (server.hasArg("chimeEnabled")) {
    config.chimeEnabled = server.arg("chimeEnabled") == "true";
  }
  if (server.hasArg("chimeMuteDark")) {
    config.chimeMuteDark = server.arg("chimeMuteDark") == "true";
  }
  saveConfig();
  getConfig();
}

void regenApPassword() {
  generateApPassword();
  if (apActive) {
    WiFi.softAP("SandClock", config.apPassword);  // applies now; joined phones must rejoin
  }
  server.send(200, "text/json", "{\"result\":\"ok\"}");
}

// Diagnostics: crash on purpose to exercise boot-loop protection (safe_mode.h).
void debugCrash() {
  String type = server.arg("type");
  if (type != "exception" && type != "wdt") {
    server.send(400, "text/plain", "400: type must be exception or wdt");
    return;
  }
  server.send(200, "text/plain", "crashing now (" + type + ")");
  delay(200);  // let the reply go out
  crashNow(type == "wdt");
}

void reboot() {
  server.send(200, F("text/html"), R""(
<!DOCTYPE html>
<html>
   <body>
      <script>
         setTimeout(function(){
            window.location.href = 'http://sandclock.local';
         }, 8000);
      </script>
      <p>Rebooting...</p>
   </body>
</html>)""); 
  delay (500);
  ESP.restart();
}

void setTimezone() {
  if(!server.hasArg("tzName") || !server.hasArg("tzValue")) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  strlcpy(config.tzName, server.arg("tzName").c_str(), sizeof(config.tzName));
  strlcpy(config.tzValue, server.arg("tzValue").c_str(), sizeof(config.tzValue));

  applyTimezone();
  saveConfig();
  getConfig();
}

void setAuth() {
  if (!server.hasArg("authPassword")) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  String pw = server.arg("authPassword");
  if (pw.length() != 0 && (pw.length() < 8 || pw.length() >= sizeof(config.authPassword))) {
    server.send(400, "text/plain", "400: password must be 8-31 characters (empty disables auth)");
    return;
  }
  strlcpy(config.authPassword, pw.c_str(), sizeof(config.authPassword));
  saveConfig();
  // the OTA page's credentials are fixed when the update server is set up at boot
  rebootPending = true;
  rebootAtMs = millis() + 1500;
  server.send(200, "text/json", "{\"result\":\"ok\",\"rebooting\":true}");
}

// Settings backup: a JSON file to keep and upload again after a flash or a
// board swap. With an admin password set, it holds the secrets in clear text.
void exportSettings() {
  String out;
  serializeJsonPretty(exportConfigDoc(), out);
  server.sendHeader("Content-Disposition", "attachment; filename=\"sandclock-settings.json\"");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}

// Body: the exported JSON as is (not a form). Reboots to apply everything,
// WiFi and the admin password included, the same way their own cards do.
void importSettings() {
  const char *err = importConfig(server.arg("plain"));
  if (err) {
    server.send(400, "text/plain", String("400: ") + err);
    return;
  }
  Log.noticeln("settings imported");
  rebootPending = true;
  rebootAtMs = millis() + 1500;
  server.send(200, "text/json", "{\"result\":\"ok\",\"rebooting\":true}");
}

void forceTemp() {
  currentTemp = server.arg("forceTemp").toInt();
  server.send(200, "text/json", "{\"result\":\"ok\"}");
}

// Debug aid: sets the system clock from a browser epoch in milliseconds. The
// next SNTP update (hourly, or via Sync Time) puts the real time back.
void forceTime() {
  // 64-bit parse: a millisecond epoch doesn't fit the 32-bit unsigned long
  unsigned long long ms = strtoull(server.arg("forceTime").c_str(), NULL, 10);
  if (ms < 1600000000000ULL) {
    server.send(400, "text/plain", "400: forceTime must be a millisecond epoch");
    return;
  }
  struct timeval tv = { (time_t)(ms / 1000), 0 };
  settimeofday(&tv, nullptr);
  server.send(200, "text/json", "{\"result\":\"ok\"}");
}

void syncTime() {
  applyTimezone();  // restarts SNTP, which syncs immediately
  server.send(200, "text/json", "{\"result\":\"ok\"}");
}

void showMessage() {
  if(!server.hasArg("messageText")) {
    server.send(400, "text/plain", "400: Invalid Request - no messageText");
    return;
  }
  long repeat = 1;
  long scrollDelay = 50;
  if(server.hasArg("repeatCount")) {
    repeat = server.arg("repeatCount").toInt();
  }
  if(server.hasArg("scrollDelay")) {
    scrollDelay = server.arg("scrollDelay").toInt();
  }
  if (repeat < 1 || repeat > 100) {
    server.send(400, "text/plain", "400: repeatCount must be 1-100");
    return;
  }
  if (scrollDelay < 10 || scrollDelay > 1000) {
    server.send(400, "text/plain", "400: scrollDelay must be 10-1000 ms");
    return;
  }

  Log.traceln("show message: %s, repeat: %d, delay: %d", server.arg("messageText").c_str(), repeat, scrollDelay);
  strlcpy(messageText, server.arg("messageText").c_str(), sizeof(messageText));
  messageScrollDelayMs = scrollDelay;
  messageRepeatCount = repeat;
  showingMessage = 1;  // set last: loop() only sees a fully prepared message
  server.send(200, "text/json", "{\"result\":\"ok\"}");
}

void getInfo() {
  DynamicJsonDocument doc(2560);
  bool up = WiFi.status() == WL_CONNECTED;

  doc["fwVersion"] = FW_VERSION;
  doc["buildDate"] = String(__DATE__) + " " + __TIME__;
  doc["gitRev"] = GIT_REV;
  doc["coreVersion"] = ESP.getCoreVersion();
  doc["sdkVersion"] = ESP.getSdkVersion();

  doc["mac"] = WiFi.macAddress();
  doc["hostname"] = "sandclock.local";
  doc["wifiMode"] = apActive ? "AP+STA" : "STA";
  doc["apActive"] = apActive;
  doc["wifiConnected"] = up;
  doc["ssid"] = up ? WiFi.SSID() : String(config.wifi_ssid);
  doc["bssid"] = up ? WiFi.BSSIDstr() : String("");
  doc["rssi"] = up ? WiFi.RSSI() : 0;
  doc["channel"] = WiFi.channel();
  doc["ip"] = (up ? WiFi.localIP() : (apActive ? WiFi.softAPIP() : IPAddress())).toString();
  doc["gateway"] = WiFi.gatewayIP().toString();
  doc["subnet"] = WiFi.subnetMask().toString();
  doc["dns"] = WiFi.dnsIP().toString();

  time_t now = time(nullptr);
  char timeBuf[40] = "";
  strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S %Z", ::localtime(&now));
  doc["timeSynced"] = timeSynced();
  doc["ntpSyncCount"] = ntpSyncCount;
  doc["ntpLastSyncAgoSec"] = ntpLastSync ? (long)(now - ntpLastSync) : -1;
  doc["localTime"] = timeBuf;
  doc["tzName"] = config.tzName;
  doc["tzValue"] = config.tzValue;

  doc["temperature"] = currentTemp;  // -1000 = no HA reading yet
  doc["haConfigured"] = hassConfigured();
  doc["haLastCode"] = haLastCode;
  doc["haLastAttemptAgoSec"] = haLastAttemptMs ? (long)((millis() - haLastAttemptMs) / 1000) : -1;
  doc["haLastOkAgoSec"] = haLastOkMs ? (long)((millis() - haLastOkMs) / 1000) : -1;
  doc["haOk"] = haLastOkMs && haLastOkMs == haLastAttemptMs;
  doc["ldr"] = sensorValue;
  doc["lightLevel"] = lightLevel;
  doc["ldrNow"] = ldrNow;                          // 20 Hz sample used by the light gesture
  doc["gestureBaseline"] = lightGesture.baseline;  // its learned light level (covers off below 60)
  doc["buzzerMuted"] = buzzerMutedByDark();
  doc["chimeEnabled"] = config.chimeEnabled;

  doc["uptimeSec"] = millis() / 1000;
  doc["resetReason"] = ESP.getResetReason();
  doc["resetInfo"] = ESP.getResetInfo();
  doc["crashCount"] = crashCount;
  doc["safeMode"] = safeMode;
  doc["apPassword"] = config.apPassword;
  doc["cpuMHz"] = ESP.getCpuFreqMHz();
  doc["chipId"] = String(ESP.getChipId(), HEX);
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["minFreeHeap"] = minFreeHeap;
  doc["maxFreeBlock"] = ESP.getMaxFreeBlockSize();
  doc["heapFragmentation"] = ESP.getHeapFragmentation();
  doc["flashSize"] = ESP.getFlashChipRealSize();
  doc["flashSpeed"] = ESP.getFlashChipSpeed();
  doc["sketchSize"] = ESP.getSketchSize();
  doc["freeSketchSpace"] = ESP.getFreeSketchSpace();

  FSInfo fs;
  if (LittleFS.info(fs)) {
    doc["fsTotal"] = fs.totalBytes;
    doc["fsUsed"] = fs.usedBytes;
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "text/json", out);
}

// Captive portal: while the setup AP is up, point any unknown address at the
// index page. Joining phones probe a well-known URL; the redirect makes them
// open the clock page by themselves. sandclock.local itself is handled by
// mDNS, which .local lookups bypass this redirect for.
void captivePortalRedirect() {
  if (apActive && (server.method() == HTTP_GET || server.method() == HTTP_HEAD)) {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "404: Not Found");
}

void setupHandlers() {
  server.on("/", HTTP_GET, secured(serveIndex));
  server.on("/index.html", HTTP_GET, secured(serveIndex));
  server.on("/info", HTTP_GET, secured(getInfo));
  server.on("/config", HTTP_GET, secured(getConfig));
  server.on("/config/hassConfig", HTTP_POST, secured(updateHassConfig));
  server.on("/config/clockcolor", HTTP_POST, secured(setClockColor));
  server.on("/config/tempcolor", HTTP_POST, secured(setTempColor));
  server.on("/config/othercolor", HTTP_POST, secured(setOtherColor));
  server.on("/config/brightness", HTTP_POST, secured(setBrightness));
  server.on("/config/autobrightness", HTTP_POST, secured(toggleAutoBrightness));
  server.on("/config/hourformat", HTTP_POST, secured(setHourFormat));
  server.on("/config/displaystyle", HTTP_POST, secured(setDisplayStyle));
  server.on("/config/chime", HTTP_POST, secured(setChime));
  server.on("/config/ap", HTTP_POST, secured(regenApPassword));
  server.on("/debug/crash", HTTP_POST, secured(debugCrash));
  server.on("/config/wifiConfig", HTTP_POST, secured(updateWifiConfig));
  server.on("/config/auth", HTTP_POST, secured(setAuth));
  server.on("/config/timezones", HTTP_POST, secured(setTimezone));
  server.on("/config/export", HTTP_GET, secured(exportSettings));
  server.on("/config/import", HTTP_POST, secured(importSettings));
  server.on("/forcetemp", HTTP_POST, secured(forceTemp));
  server.on("/forcetime", HTTP_POST, secured(forceTime));
  server.on("/syncTime", HTTP_GET, secured(syncTime));
  server.on("/reboot", HTTP_GET, secured(reboot));
  server.on("/sound", HTTP_POST, secured(playSound));
  server.on("/showmessage", HTTP_POST, secured(showMessage));
  server.onNotFound(captivePortalRedirect);
  // NOTE: /config masks wifi_password and the HA token as SECRET_MASK.
}

// Safe mode: no UI files, display or sensors, just enough to see why we are
// here and to flash a fix.
String htmlEscape(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  return s;
}

void safePage() {
  String html = F("<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
                  "<title>Safe mode</title><body style='font-family:sans-serif;max-width:40rem;margin:2rem auto;padding:0 1rem'>"
                  "<h1>Safe mode</h1><p>The clock crashed ");
  html += crashCount;
  html += F(" times in a row while starting, so it is only running WiFi and updates. "
            "Upload a working firmware, or try a normal start.</p><p>Firmware ");
  html += FW_VERSION;
  html += " (";
  html += GIT_REV;
  html += F(")<br>Last reset: ");
  html += htmlEscape(ESP.getResetInfo());
  html += F("</p><p><a href='/update'>Upload firmware or filesystem</a></p>"
            "<p><a href='/config/export'>Download the settings</a></p>"
            "<p><a href='/reboot'>Try a normal start</a></p>");
  server.send(200, "text/html", html);
}

void setupSafeHandlers() {
  server.on("/", HTTP_GET, secured(safePage));
  server.on("/info", HTTP_GET, secured(getInfo));
  server.on("/config", HTTP_GET, secured(getConfig));
  server.on("/config/export", HTTP_GET, secured(exportSettings));  // rescue the settings before reflashing
  server.on("/reboot", HTTP_GET, secured(reboot));
  server.onNotFound(captivePortalRedirect);
}
