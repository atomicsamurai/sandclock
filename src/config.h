#pragma once

#include <EEPROM.h>

#include "config_import.h"
#include "secrets.h"

void initDefaultConfig() {
  config.brightness = 128;
  config.autoBrightness = true;
  config.clock_color = RgbColor(0, 0, 255);
  config.temp_color = RgbColor(255, 0, 0);
  config.other_color = RgbColor(255, 0, 0);
  config.hourFormat = 12;
  config.displayStyle = 1;  // default: 5s/2s cycle
  config.chimeEnabled = true;
  config.chimeMuteDark = false;
  config.chimeDarkThreshold = 40;
  config.wifiMacString = WiFi.macAddress();
  strcpy(config.tzName, "Etc/UTC");
  strcpy(config.tzValue, "UTC0");
  strcpy(config.authPassword, "");
  strcpy(config.apPassword, "");
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

void loadString(char *dst, size_t size, JsonVariantConst v) {
  const char *s = v.as<const char *>();
  if (s) {
    strlcpy(dst, s, size);
  }
}

void loadColor(RgbColor &c, JsonVariantConst v) {
  c = RgbColor((v["r"] | (int)c.R) & 0xFF, (v["g"] | (int)c.G) & 0xFF, (v["b"] | (int)c.B) & 0xFF);
}

// Applies the saved JSON on top of the defaults, so a key that is missing
// (older firmware, new setting) or invalid keeps its default instead of
// becoming 0. Leaves config untouched when the JSON doesn't parse.
void applyConfigDoc(JsonDocument &doc) {
  initDefaultConfig();

  config.brightness = constrain(doc["brightness"] | (int)config.brightness, 0, 255);
  config.autoBrightness = doc["autoBrightness"] | config.autoBrightness;
  loadColor(config.clock_color, doc["clock_color"]);
  loadColor(config.temp_color, doc["temp_color"]);
  loadColor(config.other_color, doc["other_color"]);

  config.hourFormat = (doc["hourFormat"] | 12) == 24 ? 24 : 12;
  config.displayStyle = (doc["displayStyle"] | 1) == 2 ? 2 : 1;
  config.chimeEnabled = doc["chimeEnabled"] | config.chimeEnabled;
  config.chimeMuteDark = doc["chimeMuteDark"] | config.chimeMuteDark;
  config.chimeDarkThreshold = constrain(doc["chimeDarkThreshold"] | (int)config.chimeDarkThreshold, 0, 1023);
  loadString(config.hassBaseUrl, sizeof(config.hassBaseUrl), doc["hassConfig"]["hassBaseUrl"]);
  loadString(config.hassEntityId, sizeof(config.hassEntityId), doc["hassConfig"]["hassEntityId"]);
  loadString(config.hassAT, sizeof(config.hassAT), doc["hassConfig"]["hassAT"]);
  loadString(config.wifi_ssid, sizeof(config.wifi_ssid), doc["wifi_ssid"]);
  loadString(config.wifi_password, sizeof(config.wifi_password), doc["wifi_password"]);
  loadString(config.tzName, sizeof(config.tzName), doc["tzName"]);
  loadString(config.tzValue, sizeof(config.tzValue), doc["tzValue"]);
  loadString(config.authPassword, sizeof(config.authPassword), doc["authPassword"]);
  loadString(config.apPassword, sizeof(config.apPassword), doc["apPassword"]);
}

bool applyConfigJson(const char *json) {
  DynamicJsonDocument doc(2048);
  auto error = deserializeJson(doc, json);
  if (error) {
    Log.errorln("Failed to parse config");
    return false;
  }
  applyConfigDoc(doc);
  Log.traceln("Loaded config");
  return true;
}

bool loadConfig() {
  EEPROM.begin(4096);
  String json;
  if (!readEepromConfig(json)) {
    Log.errorln("No saved config");
    return false;
  }
  return applyConfigJson(json.c_str());
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
  doc["chimeEnabled"] = config.chimeEnabled;
  doc["chimeMuteDark"] = config.chimeMuteDark;
  doc["chimeDarkThreshold"] = config.chimeDarkThreshold;
  doc["fwVersion"] = FW_VERSION;
  doc["hassConfig"]["hassBaseUrl"] = config.hassBaseUrl;
  doc["hassConfig"]["hassEntityId"] = config.hassEntityId;
  doc["hassConfig"]["hassAT"] = (redact && config.hassAT[0]) ? SECRET_MASK : config.hassAT;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = (redact && config.wifi_password[0]) ? SECRET_MASK : config.wifi_password;
  doc["wifiMacString"] = config.wifiMacString;
  doc["tzName"] = config.tzName;
  doc["tzValue"] = config.tzValue;
  doc["authEnabled"] = config.authPassword[0] != 0;
  if (!redact) {
    doc["authPassword"] = config.authPassword;  // never sent over HTTP, only saved
    doc["apPassword"] = config.apPassword;
  }
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

// Setup-AP WPA2 password: random, stored, scrolled across the matrix and shown
// in the web UI. No 0/O/1/I/L so it can be read off the LEDs and typed without
// guessing (and only A-Z/2-9, which the scroll font can draw).
const char AP_PASSWORD_CHARS[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
const int AP_PASSWORD_LENGTH = 10;

void generateApPassword() {
  const int n = sizeof(AP_PASSWORD_CHARS) - 1;
  for (int i = 0; i < AP_PASSWORD_LENGTH; i++) {
    config.apPassword[i] = AP_PASSWORD_CHARS[ESP.random() % n];
  }
  config.apPassword[AP_PASSWORD_LENGTH] = 0;
  saveConfig();
}

// Settings file for download (GET /config/export). Secrets only go in when an
// admin password protects the download; see config_import.h.
DynamicJsonDocument exportConfigDoc() {
  DynamicJsonDocument doc = getConfigDoc(config.authPassword[0] == 0);
  doc[EXPORT_MARKER] = EXPORT_FORMAT;
  return doc;
}

// Replaces the settings with an uploaded export and saves them. Settings the
// file leaves out get their defaults, except secrets it doesn't carry, which
// keep their current values. Returns nullptr on success, else why it was
// refused (the config is then unchanged).
const char *importConfig(const String &json) {
  if (json.length() == 0) {
    return "empty file";
  }
  if (json.length() > IMPORT_MAX) {
    return "file too large for a settings file";
  }
  DynamicJsonDocument doc(3072);
  if (deserializeJson(doc, json)) {
    return "not valid JSON";
  }
  const char *err = checkImport(doc);
  if (err) {
    return err;
  }

  // ~1 KB: on the heap, not the 4 KB stack a web handler runs on
  std::unique_ptr<ConfigObject> saved(new ConfigObject(config));
  const ConfigObject &before = *saved;
  applyConfigDoc(doc);
  if (importKeepsCurrent(doc["wifi_ssid"])) {
    strcpy(config.wifi_ssid, before.wifi_ssid);
  }
  if (importKeepsCurrent(doc["wifi_password"])) {
    strcpy(config.wifi_password, before.wifi_password);
  }
  if (importKeepsCurrent(doc["hassConfig"]["hassAT"])) {
    strcpy(config.hassAT, before.hassAT);
  }
  if (importKeepsCurrent(doc["apPassword"])) {
    strcpy(config.apPassword, before.apPassword);
  }
  if (importKeepsAuth(doc["authPassword"])) {
    strcpy(config.authPassword, before.authPassword);
  }
  if (!saveConfig()) {
    config = before;
    return "could not save the settings";
  }
  return nullptr;
}
