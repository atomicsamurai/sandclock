// Host test for src/ha_parse.h (the same header the firmware uses).
// Built and run by tests/run.sh.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "../src/ha_parse.h"

int main() {
  std::string big =
      "{\"entity_id\":\"sensor.t\",\"state\":\"-4.5\",\"attributes\":{\"unit_of_measurement\":\"C\","
      "\"friendly_name\":\"Outdoor\",\"history\":[";
  for (int i = 0; i < 400; i++) big += (i ? ",\"x" : "\"x") + std::to_string(i) + "\"";
  big += "]},\"last_changed\":\"2026-11-02T10:00:00+00:00\"}";

  struct {
    const char *name;
    std::string body;
    bool ok;
    long temp;
  } cases[] = {
      {"plain int", "{\"state\":\"21\"}", true, 21},
      {"negative decimal", "{\"state\":\"-4.5\",\"attributes\":{\"a\":1}}", true, -4},
      {"huge attributes", big, true, -4},
      {"unavailable", "{\"state\":\"unavailable\"}", false, 0},
      {"unknown", "{\"state\":\"unknown\"}", false, 0},
      {"empty state", "{\"state\":\"\"}", false, 0},
      {"no state key", "{\"attributes\":{}}", false, 0},
      {"state is a number", "{\"state\":21}", false, 0},
      {"invalid json", "<html>502</html>", false, 0},
      {"empty body", "", false, 0},
  };

  int failures = 0;
  for (auto &c : cases) {
    long t = -1000;
    bool ok = parseHaTemperature(c.body, t);
    bool pass = ok == c.ok && (!ok || t == c.temp) && (ok || t == -1000);
    printf("  %-18s %s\n", c.name, pass ? "pass" : "FAIL");
    failures += !pass;
  }
  printf("ha_parse: %d failures\n", failures);
  return failures != 0;
}
