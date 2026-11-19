#pragma once

// Settings export/import: the rules for which files are accepted and which
// values an import keeps. Free of Arduino types so tests/ can compile it on a
// host with ArduinoJson; config.h applies the result to the live config.
//
// An export is the saved config JSON plus a format marker:
//   {"sandclockSettings": 1, "brightness": 128, ..., "wifi_password": "..."}
// Secrets (WiFi password, HA token, admin and setup-AP passwords) are only in
// it when the clock has an admin password; otherwise they are masked or left
// out, since anyone on the network could download the file.

#include <ArduinoJson.h>
#include <string.h>

const char *const EXPORT_MARKER = "sandclockSettings";
const int EXPORT_FORMAT = 1;
const size_t IMPORT_MAX = 3000;  // bytes of JSON; a full export is about 1.3 KB

// What the clock shows instead of a secret (the API's SECRET_MASK).
const char *const IMPORT_SECRET_MASK = "******";

// A secret the file doesn't carry: missing, null, empty or masked. The import
// keeps the clock's current value for these, so a file exported without
// secrets can't wipe them. (authPassword is the exception, see below.)
inline bool importKeepsCurrent(JsonVariantConst v) {
  const char *s = v.as<const char *>();
  return s == nullptr || s[0] == 0 || strcmp(s, IMPORT_SECRET_MASK) == 0;
}

struct ImportLimit {
  const char *group;  // nullptr for a top-level key
  const char *key;
  size_t maxLen;      // longest value that fits (the char array size - 1)
};

// Mirrors the char arrays in ConfigObject (vars.h). Too long is an error, not
// a truncation: a cut-off WiFi password or token just fails later, silently.
const ImportLimit IMPORT_LIMITS[] = {
    {"hassConfig", "hassBaseUrl", 99},
    {"hassConfig", "hassEntityId", 127},
    {"hassConfig", "hassAT", 511},
    {nullptr, "wifi_ssid", 31},
    {nullptr, "wifi_password", 31},
    {nullptr, "tzName", 31},
    {nullptr, "tzValue", 31},
    {nullptr, "authPassword", 31},
    {nullptr, "apPassword", 15},
};

// Checks a parsed import; returns nullptr when it can be applied, otherwise a
// message for the user.
inline const char *checkImport(JsonDocument &doc) {
  if (!doc.is<JsonObject>()) {
    return "not a settings file";
  }
  JsonVariantConst marker = doc[EXPORT_MARKER];
  if (!marker.is<int>() || marker.as<int>() < 1) {
    return "not a SandClock settings file";
  }
  if (marker.as<int>() > EXPORT_FORMAT) {
    return "settings file is from a newer firmware, update the clock first";
  }
  for (const ImportLimit &l : IMPORT_LIMITS) {
    JsonVariantConst v = l.group ? JsonVariantConst(doc[l.group][l.key]) : JsonVariantConst(doc[l.key]);
    if (v.isNull()) {
      continue;
    }
    if (!v.is<const char *>()) {
      return "a text setting has the wrong type";
    }
    if (strlen(v.as<const char *>()) > l.maxLen) {
      return "a text setting is too long";
    }
  }
  // Same rule as the Security card: 8-31 characters, or empty for no password.
  JsonVariantConst pw = doc["authPassword"];
  if (pw.is<const char *>() && strcmp(pw.as<const char *>(), IMPORT_SECRET_MASK) != 0) {
    size_t n = strlen(pw.as<const char *>());
    if (n != 0 && n < 8) {
      return "admin password in the file is shorter than 8 characters";
    }
  }
  JsonVariantConst ap = doc["apPassword"];
  if (ap.is<const char *>() && !importKeepsCurrent(ap) && strlen(ap.as<const char *>()) < 8) {
    return "setup password in the file is shorter than 8 characters";
  }
  return nullptr;
}

// The admin password is restored as exported, including "" (protection off),
// but only when the file says so explicitly: a file without the key, or with
// it masked, keeps the current one.
inline bool importKeepsAuth(JsonVariantConst v) {
  const char *s = v.as<const char *>();
  return s == nullptr || strcmp(s, IMPORT_SECRET_MASK) == 0;
}
