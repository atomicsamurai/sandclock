#!/usr/bin/env python3
"""The web UI preview (data/js/preview.js) must draw the same frames as
tools/render.py, which mirrors src/display.h. Compares them over a grid of
styles, hour formats, times and temperatures."""
import datetime
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TIMES_TEMPS = [(8, 5, -4), (13, 47, -15), (0, 30, 23), (10, 8, 7), (21, 59, -1000),
               (12, 0, 0), (9, 9, 99), (23, 15, -99)]
# every weekday, one- and two-digit days
DATES = ["2026-11-15", "2026-11-16", "2026-11-17", "2026-11-18", "2026-11-19", "2026-11-20",
         "2026-11-21", "2026-12-01", "2026-12-07", "2026-12-31"]
# message, x of its first column: entering, mid-way, leaving, lower case,
# glyph-less punctuation and a 2-byte UTF-8 character
MESSAGES = [("HELLO", 32), ("HELLO", 31), ("HELLO", 3), ("HELLO", -27), ("hello world", -20),
            ("WIFI X  IP 192.168.1.225", -50), ("A-B:C_D.E", 0), ("CAF\u00c9 OK", 2), ("?!,", 0)]


def normalize(text):
    return "\n".join(line.rstrip() for line in text.strip("\n").split("\n"))


def main():
    cases = [(s, f, h, m, t) for s in (1, 2) for f in (12, 24) for (h, m, t) in TIMES_TEMPS]
    bad = 0
    for style, fmt, h, m, t in cases:
        js = subprocess.check_output(
            ["node", os.path.join(ROOT, "tests", "preview_frames.js"),
             str(style), str(fmt), str(h), str(m), str(t)]).decode()
        js_frames = [normalize(b) for b in js.split("--\n") if b.strip()]
        py_frames = []
        for view in (["time", "temp"] if style == 1 else ["combined"]):
            out = subprocess.check_output(
                ["python3", os.path.join(ROOT, "tools", "render.py"), view,
                 "--time", f"{h:02d}:{m:02d}", "--temp", "none" if t == -1000 else str(t),
                 "--hour-format", str(fmt), "--plain"]).decode()
            py_frames.append(normalize("\n".join(out.strip("\n").split("\n")[1:])))  # drop caption
        if js_frames != py_frames:
            bad += 1
            print(f"  MISMATCH style={style} fmt={fmt} {h:02d}:{m:02d} temp={t}")
    for text, pos in MESSAGES:
        js = normalize(subprocess.check_output(
            ["node", os.path.join(ROOT, "tests", "preview_frames.js"), "message", text, str(pos)]).decode())
        out = subprocess.check_output(
            ["python3", os.path.join(ROOT, "tools", "render.py"), "message",
             "--text", text, "--pos", str(pos), "--plain"]).decode()
        py = normalize("\n".join(out.strip("\n").split("\n")[1:]))
        if js != py:
            bad += 1
            print(f"  MISMATCH message={text!r} pos={pos}")
    for iso in DATES:
        d = datetime.date.fromisoformat(iso)
        js = normalize(subprocess.check_output(
            ["node", os.path.join(ROOT, "tests", "preview_frames.js"), "date",
             str((d.weekday() + 1) % 7), str(d.day)]).decode())
        out = subprocess.check_output(
            ["python3", os.path.join(ROOT, "tools", "render.py"), "date", "--date", iso, "--plain"]).decode()
        if js != normalize("\n".join(out.strip("\n").split("\n")[1:])):
            bad += 1
            print(f"  MISMATCH date={iso}")
    print(f"preview_vs_render: {len(cases) + len(MESSAGES) + len(DATES)} cases, {bad} mismatches")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
