// Host test for src/config_import.h (the same header the firmware uses).
// Built and run by tests/run.sh.
#include <cstdio>
#include <string>

#include "../src/config_import.h"

static int failures = 0;

static void expect(const char *name, bool pass) {
  printf("  %-44s %s\n", name, pass ? "pass" : "FAIL");
  failures += !pass;
}

// nullptr = accepted
static const char *check(const std::string &json) {
  DynamicJsonDocument doc(3072);
  if (deserializeJson(doc, json)) return "not valid JSON";
  return checkImport(doc);
}

static bool accepted(const std::string &json) { return check(json) == nullptr; }

int main() {
  const std::string full =
      "{\"sandclockSettings\":1,\"brightness\":128,\"autoBrightness\":true,"
      "\"clock_color\":{\"r\":0,\"g\":0,\"b\":255},\"hourFormat\":24,"
      "\"hassConfig\":{\"hassBaseUrl\":\"http://ha:8123\",\"hassEntityId\":\"sensor.t\",\"hassAT\":\"" +
      std::string(200, 'x') +
      "\"},\"wifi_ssid\":\"home\",\"wifi_password\":\"secret123\",\"tzName\":\"Europe/Berlin\","
      "\"tzValue\":\"CET-1CEST,M3.5.0,M10.5.0/3\",\"authPassword\":\"adminpass\",\"apPassword\":\"ABCDEFGH23\"}";

  expect("full export", accepted(full));
  expect("redacted export (masks, no passwords)",
         accepted("{\"sandclockSettings\":1,\"wifi_password\":\"******\",\"hassConfig\":{\"hassAT\":\"******\"}}"));
  expect("marker only", accepted("{\"sandclockSettings\":1}"));
  expect("empty admin password (protection off)", accepted("{\"sandclockSettings\":1,\"authPassword\":\"\"}"));

  expect("not JSON", !accepted("hello"));
  expect("array", !accepted("[1,2]"));
  expect("no marker (e.g. a /config or /info dump)", !accepted("{\"brightness\":10}"));
  expect("newer format", !accepted("{\"sandclockSettings\":2}"));
  expect("marker is a string", !accepted("{\"sandclockSettings\":\"1\"}"));
  expect("wifi password 32 chars",
         !accepted("{\"sandclockSettings\":1,\"wifi_password\":\"" + std::string(32, 'p') + "\"}"));
  expect("wifi password 31 chars",
         accepted("{\"sandclockSettings\":1,\"wifi_password\":\"" + std::string(31, 'p') + "\"}"));
  expect("HA token 512 chars",
         !accepted("{\"sandclockSettings\":1,\"hassConfig\":{\"hassAT\":\"" + std::string(512, 't') + "\"}}"));
  expect("HA token 511 chars",
         accepted("{\"sandclockSettings\":1,\"hassConfig\":{\"hassAT\":\"" + std::string(511, 't') + "\"}}"));
  expect("ssid is a number", !accepted("{\"sandclockSettings\":1,\"wifi_ssid\":5}"));
  expect("admin password 7 chars", !accepted("{\"sandclockSettings\":1,\"authPassword\":\"1234567\"}"));
  expect("setup password 7 chars", !accepted("{\"sandclockSettings\":1,\"apPassword\":\"ABCDEFG\"}"));

  DynamicJsonDocument doc(512);
  deserializeJson(doc, "{\"a\":\"\",\"b\":\"******\",\"c\":\"x\",\"n\":null}");
  expect("keep current: missing", importKeepsCurrent(doc["zzz"]));
  expect("keep current: null", importKeepsCurrent(doc["n"]));
  expect("keep current: empty", importKeepsCurrent(doc["a"]));
  expect("keep current: masked", importKeepsCurrent(doc["b"]));
  expect("replace: real value", !importKeepsCurrent(doc["c"]));
  expect("admin password: missing keeps", importKeepsAuth(doc["zzz"]));
  expect("admin password: masked keeps", importKeepsAuth(doc["b"]));
  expect("admin password: empty turns protection off", !importKeepsAuth(doc["a"]));

  printf(failures ? "%d FAILED\n" : "all passed\n", failures);
  return failures ? 1 : 0;
}
