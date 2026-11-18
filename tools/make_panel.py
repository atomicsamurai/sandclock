#!/usr/bin/env python3
"""Make data/img/panel.jpg, the wood background of the web UI preview, from
docs/clock-temperature.jpg (the clock showing 0 degrees in cyan).

The lit LEDs (columns 22-31) are painted out row by row: the wood grain runs
along the rows, so each masked run is filled by linear interpolation along its
row, plus fine texture taken from blank wood further left on the same row.
The result is cropped to the inside of the panel and scaled to 768 px wide.

If the photo or crop changes, update PANEL in data/js/preview.js: the LED grid
in the source photo has column 0 at x=121, row 0 at y=130, and a pitch of
49.5 px across and 49 px down.

Needs Pillow, numpy and scipy (e.g. in a throwaway venv).
"""
import argparse
import os

import numpy as np
from PIL import Image
from scipy import ndimage as nd

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIT = (95, 510, 1150, 1680)      # y0, y1, x0, x1 of the lit LEDs and glow
CROP = (80, 70, 1910, 535)       # x0, y0, x1, y1 inside the panel edges
WIDTH = 768


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--src", default=os.path.join(ROOT, "docs/clock-temperature.jpg"))
    ap.add_argument("--out", default=os.path.join(ROOT, "data/img/panel.jpg"))
    args = ap.parse_args()

    a = np.asarray(Image.open(args.src).convert("RGB")).astype(float)
    ratio = a[..., 2] / (a[..., 0] + 1)        # cyan glow raises blue vs red
    base = np.median(ratio[100:520, 150:1100])  # blank wood
    mask = ratio > base + 0.04
    roi = np.zeros_like(mask)
    y0, y1, x0, x1 = LIT
    roi[y0:y1, x0:x1] = True
    mask = nd.binary_dilation(mask & roi, iterations=12)

    lo = a.copy()
    xs = np.arange(a.shape[1])
    for y in np.where(mask.any(axis=1))[0]:
        m = mask[y]
        for c in range(3):
            lo[y, m, c] = np.interp(xs[m], xs[~m], a[y, ~m, c])
    texture = a - nd.uniform_filter(a, size=(1, 31, 1))
    fill = lo + np.roll(texture, 600, axis=1)
    soft = nd.gaussian_filter(mask.astype(float), 4)[..., None]
    res = np.clip(a * (1 - soft) + fill * soft, 0, 255).astype("uint8")

    img = Image.fromarray(res).crop(CROP)
    img = img.resize((WIDTH, round(img.height * WIDTH / img.width)), Image.LANCZOS)
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    img.save(args.out, quality=75, optimize=True, progressive=True)
    print(f"{args.out}: {img.width}x{img.height}, {os.path.getsize(args.out)} bytes")


if __name__ == "__main__":
    main()
