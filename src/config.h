#pragma once

#include "secrets.h"

void initDefaultConfig() {
  config.brightness = 128;
  config.autoBrightness = true;
  config.clock_color = RgbColor(0, 0, 255);
  config.temp_color = RgbColor(255, 0, 0);
  config.other_color = RgbColor(255, 0, 0);
  config.hourFormat = 12;
  config.wifiMacString = WiFi.macAddress();
  strcpy(config.tzName, "Etc/UTC");
  strcpy(config.tzValue, "UTC0");
  strcpy(config.hassBaseUrl, "");
  strcpy(config.hassEntityId, "");
  strcpy(config.hassAT, "");
  strcpy(config.wifi_ssid, WIFI_SSID);
  strcpy(config.wifi_password, WIFI_PASSWORD);
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Log.errorln("Failed to open config file");
    return false;
  }
  config.wifiMacString = WiFi.macAddress();
  size_t size = configFile.size();
  if (size > 1024) {
    Log.errorln("Config file size is too large");
    return false;
  }
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  DynamicJsonDocument doc(2048);
  auto error = deserializeJson(doc, buf.get());
  if (error) {
    Log.errorln("Failed to parse config file");
    return false;
  }
  else {
    config.brightness = doc["brightness"];
    config.autoBrightness = doc["autoBrightness"];

    config.clock_color = RgbColor(doc["clock_color"]["r"], doc["clock_color"]["g"], doc["clock_color"]["b"]);
    config.temp_color = RgbColor(doc["temp_color"]["r"], doc["temp_color"]["g"], doc["temp_color"]["b"]);
    config.other_color = RgbColor(doc["other_color"]["r"], doc["other_color"]["g"], doc["other_color"]["b"]);

    config.hourFormat = doc["hourFormat"];
    strlcpy(config.hassBaseUrl, doc["hassConfig"]["hassBaseUrl"], sizeof(config.hassBaseUrl));
    strlcpy(config.hassEntityId, doc["hassConfig"]["hassEntityId"], sizeof(config.hassEntityId));
    strlcpy(config.hassAT, doc["hassConfig"]["hassAT"], sizeof(config.hassAT));
    strlcpy(config.wifi_ssid, doc["wifi_ssid"], sizeof(config.wifi_ssid));
    strlcpy(config.wifi_password, doc["wifi_password"], sizeof(config.wifi_password));

    strlcpy(config.tzName, doc["tzName"], sizeof(config.tzName));
    strlcpy(config.tzValue, doc["tzValue"], sizeof(config.tzValue));
    // config.tzName = doc["tzName"];
    // config.tzValue = doc["tzValue"];
  }

  char buffer[1024]; 
  serializeJson(doc, buffer);
  Log.traceln("Loaded config: %s", buffer);

  return true;
}

DynamicJsonDocument getConfigDoc() {
  DynamicJsonDocument doc(2048);
  doc["brightness"] = config.brightness;
  doc["autoBrightness"] = config.autoBrightness;

  doc["clock_color"]["r"] = config.clock_color.R;
  doc["clock_color"]["g"] = config.clock_color.G;
  doc["clock_color"]["b"] = config.clock_color.B;
  doc["temp_color"]["r"] = config.temp_color.R;
  doc["temp_color"]["g"] = config.temp_color.G;
  doc["temp_color"]["b"] = config.temp_color.B;
  doc["other_color"]["r"] = config.other_color.R;
  doc["other_color"]["g"] = config.other_color.G;
  doc["other_color"]["b"] = config.other_color.B;

  doc["hourFormat"] = config.hourFormat;
  doc["hassConfig"]["hassBaseUrl"] = config.hassBaseUrl;
  doc["hassConfig"]["hassEntityId"] = config.hassEntityId;
  doc["hassConfig"]["hassAT"] = config.hassAT;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["wifiMacString"] = config.wifiMacString;
  doc["tzName"] = config.tzName;
  doc["tzValue"] = config.tzValue;
  return doc;
}

void saveConfig() {
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Log.errorln("Failed to open config file for writing");
    return;
  }
  serializeJson(getConfigDoc(), configFile);
  Log.traceln("Saved config");
  return;
}
