#pragma once

#include "config.h"
#include "hass.h"
#include "scroll.h"
#include "sounds.h"
#include "time_ops.h"
#include "vars.h"

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
  if(!server.hasArg("displayStyle")) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  long s = server.arg("displayStyle").toInt();
  if (s != 1 && s != 2) {
    server.send(400, "text/plain", "400: displayStyle must be 1 (cycle) or 2 (combined)");
    return;
  }
  config.displayStyle = s;
  Log.traceln("set display style: %d", s);
  saveConfig();
  getConfig();
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

void forceTemp() {
  currentTemp = server.arg("forceTemp").toInt();
  server.send(200, "text/json", "{\"result\":\"ok\"}");
}

void forceTime() {
  setTime(strtoul(server.arg("forceTime").c_str(), NULL, 0)/1000);
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
  DynamicJsonDocument doc(2048);
  bool up = WiFi.status() == WL_CONNECTED;

  doc["fwVersion"] = FW_VERSION;
  doc["buildDate"] = String(__DATE__) + " " + __TIME__;
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
  doc["haConfigured"] = config.hassAT[0] && config.hassBaseUrl[0] && config.hassEntityId[0];
  doc["haLastCode"] = haLastCode;
  doc["haLastAttemptAgoSec"] = haLastAttemptMs ? (long)((millis() - haLastAttemptMs) / 1000) : -1;
  doc["haLastOkAgoSec"] = haLastOkMs ? (long)((millis() - haLastOkMs) / 1000) : -1;
  doc["haOk"] = haLastOkMs && haLastOkMs == haLastAttemptMs;
  doc["ldr"] = sensorValue;

  doc["uptimeSec"] = millis() / 1000;
  doc["resetReason"] = ESP.getResetReason();
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

void setupHandlers() {
  server.on("/info", HTTP_GET, getInfo);
  server.on("/config", HTTP_GET, getConfig);
  server.on("/config/hassConfig", HTTP_POST, updateHassConfig);
  server.on("/config/clockcolor", HTTP_POST, setClockColor);
  server.on("/config/tempcolor", HTTP_POST, setTempColor);
  server.on("/config/othercolor", HTTP_POST, setOtherColor);
  server.on("/config/brightness", HTTP_POST, setBrightness);
  server.on("/config/autobrightness", HTTP_POST, toggleAutoBrightness);
  server.on("/config/hourformat", HTTP_POST, setHourFormat);
  server.on("/config/displaystyle", HTTP_POST, setDisplayStyle);
  server.on("/config/wifiConfig", HTTP_POST, updateWifiConfig);
  server.on("/config/timezones", HTTP_POST, setTimezone);
  server.on("/forcetemp", HTTP_POST, forceTemp);
  server.on("/forcetime", HTTP_POST, forceTime);
  server.on("/syncTime", HTTP_GET, syncTime);
  server.on("/reboot", HTTP_GET, reboot);
  server.on("/sound", HTTP_POST, playSound);
  server.on("/showmessage", HTTP_POST, showMessage);
  // NOTE: /config masks wifi_password and the HA token as SECRET_MASK.
}
