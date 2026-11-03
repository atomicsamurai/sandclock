#!/usr/bin/env bash
# Build the firmware and filesystem image, flash them to the clock over OTA,
# and verify the result. Config lives in EEPROM, so neither flash touches it;
# the script compares /config before and after to prove that.
#
# Usage: tools/deploy.sh [--fw] [--fs] [--no-build] [--dry-run] [--host HOST]
#   With neither --fw nor --fs, both are flashed (firmware first).
#   Host defaults to $CLOCK_HOST or sandclock.local.
#   If password protection is on:  CLOCK_PASSWORD=... tools/deploy.sh
set -euo pipefail

cd "$(dirname "$0")/.."

HOST="${CLOCK_HOST:-sandclock.local}"
DO_FW=0
DO_FS=0
BUILD=1
DRY=0
FW_BIN=.pio/build/d1_mini/firmware.bin
FS_BIN=.pio/build/d1_mini/littlefs.bin

while [[ $# -gt 0 ]]; do
  case "$1" in
    --fw) DO_FW=1 ;;
    --fs) DO_FS=1 ;;
    --no-build) BUILD=0 ;;
    --dry-run) DRY=1 ;;
    --host) HOST="$2"; shift ;;
    -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
  shift
done
if (( !DO_FW && !DO_FS )); then DO_FW=1; DO_FS=1; fi

die() { echo "error: $*" >&2; exit 1; }
for tool in curl jq pio git; do
  command -v "$tool" >/dev/null || die "$tool is required"
done

# curl against the clock; the password goes through a config on stdin so it
# never shows up in the process list.
cc() {
  if [[ -n "${CLOCK_PASSWORD:-}" ]]; then
    local pw="${CLOCK_PASSWORD//\\/\\\\}"
    pw="${pw//\"/\\\"}"
    printf 'user = "admin:%s"\n' "$pw" | curl -sS -K - "$@"
  else
    curl -sS "$@"
  fi
}

# Reads retry because mDNS lookups of sandclock.local time out now and then.
# Uploads must not retry: a repeat POST could hit the clock while it reboots.
get() { cc -m 10 -f --retry 3 --retry-all-errors --retry-delay 1 "$@"; }

info_field() { get "http://$HOST/info" | jq -r ".$1"; }

# Stable view of /config for before/after comparison (brightness moves with
# auto-brightness; fwVersion changes by design).
config_snapshot() { get "http://$HOST/config" | jq -S 'del(.fwVersion, .brightness)'; }

wait_restart() { # $1 = uptime (s) read before the upload
  local before=$1 deadline=$((SECONDS + 120)) up
  while (( SECONDS < deadline )); do
    up=$(info_field uptimeSec 2>/dev/null || true)
    if [[ "$up" =~ ^[0-9]+$ ]] && (( up < before )); then return 0; fi
    sleep 3
  done
  die "clock did not come back within 120s"
}

upload() { # $1 = form field (firmware|filesystem), $2 = file
  local out
  out=$(cc -m 300 -F "$1=@$2" "http://$HOST/update") || die "$1 upload failed"
  [[ "$out" == *"Update Success"* ]] || die "$1 upload rejected: $out"
}

EXPECT_VER=$(sed -n 's/^#define FW_VERSION "\(.*\)"/\1/p' src/vars.h)
EXPECT_REV=$(git describe --always --dirty --abbrev=7)

code=$(cc -m 10 --retry 3 --retry-all-errors --retry-delay 1 -o /dev/null -w '%{http_code}' "http://$HOST/info" || true)
case "$code" in
  200) ;;
  401) die "password required: set CLOCK_PASSWORD" ;;
  *) die "clock not reachable at $HOST (HTTP $code)" ;;
esac
INFO=$(get "http://$HOST/info") || die "could not read /info from $HOST"
echo "clock: running $(jq -r .fwVersion <<<"$INFO") ($(jq -r '.gitRev // "pre-2.8"' <<<"$INFO")), uptime $(jq -r .uptimeSec <<<"$INFO")s"
[[ "$(jq -r '.safeMode // false' <<<"$INFO")" == "true" ]] &&
  echo "NOTE: the clock is in SAFE MODE ($(jq -r .crashCount <<<"$INFO") crashed starts); flashing a fixed firmware restores normal operation"
echo "deploy: $EXPECT_VER ($EXPECT_REV)  fw=$DO_FW fs=$DO_FS"

if (( DRY )); then
  echo "dry run: would build=$BUILD and flash to $HOST, nothing changed"
  exit 0
fi

if (( BUILD )); then
  (( DO_FW )) && pio run
  (( DO_FS )) && pio run -t buildfs
fi
(( DO_FW )) && [[ ! -f "$FW_BIN" ]] && die "missing $FW_BIN (run without --no-build)"
(( DO_FS )) && [[ ! -f "$FS_BIN" ]] && die "missing $FS_BIN (run without --no-build)"

BEFORE=$(mktemp)
trap 'rm -f "$BEFORE"' EXIT
config_snapshot > "$BEFORE"

if (( DO_FW )); then
  up=$(info_field uptimeSec)
  echo "flashing firmware ($(wc -c < "$FW_BIN") bytes)..."
  upload firmware "$FW_BIN"
  wait_restart "$up"
  got=$(info_field gitRev)
  [[ "$got" == "$EXPECT_REV" ]] || die "firmware revision is $got, expected $EXPECT_REV"
  echo "firmware ok: $(info_field fwVersion) ($got)"
fi

if (( DO_FS )); then
  up=$(info_field uptimeSec)
  echo "flashing filesystem ($(wc -c < "$FS_BIN") bytes, about a minute)..."
  upload filesystem "$FS_BIN"
  wait_restart "$up"
  idx=$(cc -m 10 --retry 3 --retry-all-errors --retry-delay 1 -o /dev/null -w '%{http_code}' "http://$HOST/" || true)
  [[ "$idx" == "200" ]] || die "index page returned HTTP $idx after filesystem flash"
  echo "filesystem ok: index page served, $(info_field fsUsed) bytes used"
fi

# Compare only keys the old firmware already reported; a new firmware may
# add fields (e.g. authEnabled), which isn't a config change.
AFTER=$(mktemp)
trap 'rm -f "$BEFORE" "$AFTER"' EXIT
config_snapshot | jq -S --slurpfile b "$BEFORE" 'with_entries(select(.key as $k | $b[0] | has($k)))' > "$AFTER"
if diff "$BEFORE" "$AFTER" >/dev/null; then
  echo "config unchanged"
else
  echo "WARNING: config differs after deploy:" >&2
  diff "$BEFORE" "$AFTER" >&2 || true
  exit 1
fi
echo "done"
