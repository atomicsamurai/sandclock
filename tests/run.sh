#!/usr/bin/env bash
# Host-side checks (no clock needed). Run from anywhere: tests/run.sh
# The C++ test needs ArduinoJson; `pio run` once fetches it, or point
# ARDUINOJSON_DIR at a copy of its src/ directory.
set -euo pipefail
cd "$(dirname "$0")/.."

step() { printf '\n== %s\n' "$*"; }

step "JavaScript syntax"
for f in data/js/*.js tests/preview_frames.js; do node --check "$f"; done
echo "ok"

step "Shell syntax"
bash -n tools/deploy.sh
bash -n tests/run.sh
echo "ok"

step "fonts.js matches src/digits.h"
python3 tools/gen_fonts_js.py --check

step "Web UI preview vs tools/render.py"
python3 tests/preview_vs_render.py

step "Home Assistant reply parsing (src/ha_parse.h)"
AJ="${ARDUINOJSON_DIR:-.pio/libdeps/d1_mini/ArduinoJson/src}"
[[ -f "$AJ/ArduinoJson.h" ]] || { echo "ArduinoJson not found at $AJ (run 'pio run' once, or set ARDUINOJSON_DIR)" >&2; exit 1; }
BIN="$(mktemp -d)/ha_parse_test"
${CXX:-c++} -std=c++17 -I"$AJ" tests/ha_parse_test.cpp -o "$BIN"
"$BIN"

step "Safe mode decision logic (src/safe_mode_logic.h)"
BIN2="$(mktemp -d)/safe_mode_test"
${CXX:-c++} -std=c++17 tests/safe_mode_test.cpp -o "$BIN2"
"$BIN2"

step "Light-sensor gesture (src/light_gesture_logic.h)"
BIN3="$(mktemp -d)/light_gesture_test"
${CXX:-c++} -std=c++17 tests/light_gesture_test.cpp -o "$BIN3"
"$BIN3"

step "Date screen layout (src/date_screen_logic.h)"
BIN5="$(mktemp -d)/date_screen_test"
${CXX:-c++} -std=c++17 tests/date_screen_test.cpp -o "$BIN5"
"$BIN5"

step "Message queue (src/message_queue_logic.h)"
BIN6="$(mktemp -d)/message_queue_test"
${CXX:-c++} -std=c++17 tests/message_queue_test.cpp -o "$BIN6"
"$BIN6"

step "Settings import checks (src/config_import.h)"
BIN4="$(mktemp -d)/config_import_test"
${CXX:-c++} -std=c++17 -I"$AJ" tests/config_import_test.cpp -o "$BIN4"
"$BIN4"

printf '\nall checks passed\n'
