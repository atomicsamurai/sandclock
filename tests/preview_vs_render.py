#!/usr/bin/env python3
"""The web UI preview (data/js/preview.js) must draw the same frames as
tools/render.py, which mirrors src/display.h. Compares them over a grid of
styles, hour formats, times and temperatures."""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TIMES_TEMPS = [(8, 5, -4), (13, 47, -15), (0, 30, 23), (10, 8, 7), (21, 59, -1000),
               (12, 0, 0), (9, 9, 99), (23, 15, -99)]


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
    print(f"preview_vs_render: {len(cases)} cases, {bad} mismatches")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
