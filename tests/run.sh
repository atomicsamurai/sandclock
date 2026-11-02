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

printf '\nall checks passed\n'
