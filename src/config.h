#pragma once

#include <EEPROM.h>

#include "secrets.h"

void initDefaultConfig() {
  config.brightness = 128;
  config.autoBrightness = true;
  config.clock_color = RgbColor(0, 0, 255);
  config.temp_color = RgbColor(255, 0, 0);
  config.other_color = RgbColor(255, 0, 0);
  config.hourFormat = 12;
  config.displayStyle = 1;  // default: 5s/2s cycle
  config.wifiMacString = WiFi.macAddress();
  strcpy(config.tzName, "Etc/UTC");
  strcpy(config.tzValue, "UTC0");
  strcpy(config.hassBaseUrl, "");
  strcpy(config.hassEntityId, "");
  strcpy(config.hassAT, "");
  strcpy(config.wifi_ssid, WIFI_SSID);
  strcpy(config.wifi_password, WIFI_PASSWORD);
}

/*
 * Config lives in the EEPROM sector (just past the filesystem in flash), not
 * in the filesystem: flashing a filesystem image or OTA-updating the firmware
 * leaves it intact. Layout: magic, payload length, CRC32 of payload (all
 * uint32 little-endian), then the config JSON.
 */
const uint32_t CFG_MAGIC = 0xC10C0CF6;
const size_t CFG_HEADER = 12;
const size_t CFG_MAX = 2000;
const char *const LEGACY_CONFIG_FILE = "/config.json";

bool saveConfig();

uint32_t configCrc32(const char *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  while (len--) {
    crc ^= (uint8_t)*data++;
    for (int k = 0; k < 8; k++) {
      crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
  }
  return ~crc;
}

bool readEepromConfig(String &json) {
  const uint8_t *d = EEPROM.getConstDataPtr();
  uint32_t magic, len, crc;
  memcpy(&magic, d, 4);
  memcpy(&len, d + 4, 4);
  memcpy(&crc, d + 8, 4);
  if (magic != CFG_MAGIC || len == 0 || len > CFG_MAX) {
    return false;
  }
  json = "";
  json.concat((const char *)(d + CFG_HEADER), len);
  return configCrc32(json.c_str(), len) == crc;
}

bool applyConfigJson(const char *json) {
  DynamicJsonDocument doc(2048);
  auto error = deserializeJson(doc, json);
  if (error) {
    Log.errorln("Failed to parse config");
    return false;
  }

  config.brightness = doc["brightness"];
  config.autoBrightness = doc["autoBrightness"];

  config.clock_color = RgbColor(doc["clock_color"]["r"], doc["clock_color"]["g"], doc["clock_color"]["b"]);
  config.temp_color = RgbColor(doc["temp_color"]["r"], doc["temp_color"]["g"], doc["temp_color"]["b"]);
  config.other_color = RgbColor(doc["other_color"]["r"], doc["other_color"]["g"], doc["other_color"]["b"]);

  config.hourFormat = doc["hourFormat"];
  // default for configs saved before displayStyle existed
  config.displayStyle = doc["displayStyle"] | 1;
  strlcpy(config.hassBaseUrl, doc["hassConfig"]["hassBaseUrl"], sizeof(config.hassBaseUrl));
  strlcpy(config.hassEntityId, doc["hassConfig"]["hassEntityId"], sizeof(config.hassEntityId));
  strlcpy(config.hassAT, doc["hassConfig"]["hassAT"], sizeof(config.hassAT));
  strlcpy(config.wifi_ssid, doc["wifi_ssid"], sizeof(config.wifi_ssid));
  strlcpy(config.wifi_password, doc["wifi_password"], sizeof(config.wifi_password));

  strlcpy(config.tzName, doc["tzName"], sizeof(config.tzName));
  strlcpy(config.tzValue, doc["tzValue"], sizeof(config.tzValue));

  char buffer[1024];
  serializeJson(doc, buffer);
  Log.traceln("Loaded config: %s", buffer);
  return true;
}

// One-time import of the pre-EEPROM /config.json. The file is removed only
// after the EEPROM commit succeeds, so a failed migration can be retried.
bool migrateLegacyConfig() {
  if (!SPIFFS.exists(LEGACY_CONFIG_FILE)) {
    return false;
  }
  File f = SPIFFS.open(LEGACY_CONFIG_FILE, "r");
  if (!f || f.size() == 0 || f.size() > CFG_MAX) {
    Log.errorln("Legacy config unreadable or too large");
    return false;
  }
  String json = f.readString();
  f.close();
  if (!applyConfigJson(json.c_str())) {
    return false;
  }
  if (saveConfig()) {
    SPIFFS.remove(LEGACY_CONFIG_FILE);
    Log.noticeln("Migrated config.json to EEPROM");
  }
  return true;
}

bool loadConfig() {
  EEPROM.begin(4096);
  config.wifiMacString = WiFi.macAddress();
  String json;
  if (readEepromConfig(json)) {
    return applyConfigJson(json.c_str());
  }
  if (migrateLegacyConfig()) {
    return true;
  }
  Log.errorln("No saved config");
  return false;
}

// Placeholder the API returns instead of the wifi password and HA token. The
// POST handlers treat it as "unchanged" so a UI round trip can't overwrite
// the real secret.
const char *const SECRET_MASK = "******";

// redact=true is for the HTTP API; saveConfig() needs the real values.
DynamicJsonDocument getConfigDoc(bool redact = false) {
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
  doc["displayStyle"] = config.displayStyle;
  doc["fwVersion"] = FW_VERSION;
  doc["hassConfig"]["hassBaseUrl"] = config.hassBaseUrl;
  doc["hassConfig"]["hassEntityId"] = config.hassEntityId;
  doc["hassConfig"]["hassAT"] = (redact && config.hassAT[0]) ? SECRET_MASK : config.hassAT;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = (redact && config.wifi_password[0]) ? SECRET_MASK : config.wifi_password;
  doc["wifiMacString"] = config.wifiMacString;
  doc["tzName"] = config.tzName;
  doc["tzValue"] = config.tzValue;
  return doc;
}

void eepromWrite(size_t addr, const void *src, size_t len) {
  const uint8_t *p = (const uint8_t *)src;
  for (size_t i = 0; i < len; i++) {
    EEPROM.write(addr + i, p[i]);  // only marks dirty when the byte changes
  }
}

bool saveConfig() {
  String json;
  serializeJson(getConfigDoc(), json);
  if (json.length() > CFG_MAX) {
    Log.errorln("Config too large to save (%d bytes)", json.length());
    return false;
  }
  uint32_t magic = CFG_MAGIC;
  uint32_t len = json.length();
  uint32_t crc = configCrc32(json.c_str(), len);
  eepromWrite(0, &magic, 4);
  eepromWrite(4, &len, 4);
  eepromWrite(8, &crc, 4);
  eepromWrite(CFG_HEADER, json.c_str(), len);
  if (!EEPROM.commit()) {
    Log.errorln("EEPROM commit failed");
    return false;
  }
  Log.traceln("Saved config");
  return true;
}
