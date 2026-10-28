#!/usr/bin/env python3
"""Terminal mock of the 32x8 matrix. Glyph data is parsed from src/digits.h.

The layouts below mirror showTime/showTemperature/showTemperatureSmall/
showCombined in src/display.h; update both when one changes.
"""
import argparse
import os
import re
import sys
import time

W, H = 32, 8
COLON, BLANK, PERIOD, DEGREES, MINUS = 10, 11, 12, 13, 14  # Temp/Compact indices
UB_MINUS = 11  # MINUS index in the UserBold font
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_fonts(path):
    src = open(path).read()
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    start = src.index("font_names")
    names = re.findall(r'"(\w+)"', src[start:src.index("};", start)])
    body = src[src.index("const Font fonts[]"):]
    token = re.compile(
        r"(?P<font>Font\s*\{\s*(?P<h>\d+)\s*,)"
        r"|(?P<glyph>\{\s*(?P<w>\d+)\s*,\s*\{(?P<d>[^{}]*)\}\s*\})"
    )
    fonts = []
    for m in token.finditer(body):
        if m.group("font"):
            fonts.append({"height": int(m.group("h")), "glyphs": []})
        else:
            rows = [int(b, 2) for b in re.findall(r"0b([01]+)", m.group("d"))]
            fonts[-1]["glyphs"].append({"width": int(m.group("w")), "rows": rows})
    if len(fonts) != len(names):
        sys.exit(f"parsed {len(fonts)} fonts but {len(names)} names in {path}")
    return dict(zip(names, fonts))


class Canvas:
    def __init__(self):
        self.px = [[None] * W for _ in range(H)]

    def print_at(self, x, y, idx, font, color):
        glyph = font["glyphs"][idx]
        w = glyph["width"]
        for j, row in enumerate(glyph["rows"]):
            for i in range(w):
                if row & (1 << (w - 1 - i)):
                    px, py = x + i, y + j
                    if 0 <= px < W and 0 <= py < H:
                        self.px[py][px] = color


def to_12h(h, fmt):
    if fmt == 12:
        return 12 if h == 0 else h - 12 if h > 12 else h
    return h


def show_time(c, fonts, h, m, dots, clock):
    font = fonts["UserBold"]
    h1, h2, m1, m2 = h // 10, h % 10, m // 10, m % 10
    colon = clock if dots else None
    if h1 > 0:
        c.print_at(1, 0, h1, font, clock)
        c.print_at(8, 0, h2, font, clock)
        c.print_at(15, 0, COLON, font, colon)
        c.print_at(18, 0, m1, font, clock)
        c.print_at(25, 0, m2, font, clock)
    else:
        c.print_at(2, 0, h2, font, clock)
        c.print_at(10, 0, COLON, font, colon)
        c.print_at(14, 0, m1, font, clock)
        c.print_at(21, 0, m2, font, clock)


def show_temperature(c, fonts, t, color):
    font = fonts["UserBold"]
    glyphs, slots, degree_slot = [], [], -1
    if t == -1000:
        slots = [1, 8, 18]
        glyphs = [UB_MINUS] * 3
    else:
        p = min(abs(t), 99)
        t1, t2 = p // 10, p % 10
        if t < 0:
            slots = [1, 8, 18]
            glyphs.append(UB_MINUS)
            if t1 > 0:
                glyphs.append(t1)
            glyphs.append(t2)
            degree_slot = slots[len(glyphs) - 1] + 7
        elif t1 > 0:
            slots = [1, 8, 15]
            glyphs = [t1, t2]
            degree_slot = 22
        else:
            slots = [1]
            glyphs = [t2]
            degree_slot = 8
    for g, s in zip(glyphs, slots):
        c.print_at(s, 0, g, font, color)
    if degree_slot >= 0:
        c.print_at(degree_slot, 0, DEGREES, font, color)


def show_small(c, fonts, t, color):
    font = fonts["Temp"]
    x0, y0 = 22, 1
    if t == -1000:
        for dx in (0, 3, 7):
            c.print_at(x0 + dx, y0, MINUS, font, color)
        return
    p = min(abs(t), 99)
    t1, t2 = p // 10, p % 10
    if t < 0:
        c.print_at(x0, y0, MINUS, font, color)
        if t1 > 0:
            c.print_at(x0 + 3, y0, t1, font, color)
            c.print_at(x0 + 7, y0, t2, font, color)
        else:
            c.print_at(x0 + 3, y0, t2, font, color)
            c.print_at(x0 + 8, y0, DEGREES, font, color)
    elif t1 > 0:
        c.print_at(x0, y0, t1, font, color)
        c.print_at(x0 + 4, y0, t2, font, color)
        c.print_at(x0 + 8, y0, DEGREES, font, color)
    else:
        c.print_at(x0, y0, t2, font, color)
        c.print_at(x0 + 4, y0, DEGREES, font, color)


def show_combined(c, fonts, h, m, t, dots, clock, temp):
    font = fonts["Compact"]
    h1, h2, m1, m2 = h // 10, h % 10, m // 10, m % 10
    if h1 > 0:
        c.print_at(0, 0, h1, font, clock)
    c.print_at(5, 0, h2, font, clock)
    c.print_at(10, 0, COLON, font, clock if dots else None)
    c.print_at(13, 0, m1, font, clock)
    c.print_at(18, 0, m2, font, clock)
    show_small(c, fonts, t, temp)


def draw(c, plain, ruler):
    out = []
    if ruler:
        out.append("".join(f"{x % 10} " for x in range(W)))
    for row in c.px:
        line = []
        for color in row:
            if plain:
                line.append("##" if color else ". ")
            elif color:
                r, g, b = color
                line.append(f"\x1b[48;2;{r};{g};{b}m  \x1b[0m")
            else:
                line.append("\x1b[38;2;70;70;70m· \x1b[0m")
        out.append("".join(line))
    return "\n".join(out)


def rgb(s):
    try:
        r, g, b = (int(v) for v in s.split(","))
        assert all(0 <= v <= 255 for v in (r, g, b))
        return (r, g, b)
    except Exception:
        raise argparse.ArgumentTypeError("expected r,g,b with values 0-255")


def temp_arg(s):
    return -1000 if s.lower() in ("none", "fail", "--") else int(s)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("view", nargs="?", default="all",
                    choices=["time", "temp", "small", "combined", "all"])
    ap.add_argument("--time", help="HH:MM (default: now)")
    ap.add_argument("--temp", type=temp_arg, default=21,
                    help="integer degrees, or 'none' for the HA-failure dashes")
    ap.add_argument("--dots", choices=["on", "off"], default="on", help="colon state")
    ap.add_argument("--hour-format", type=int, choices=[12, 24], default=24)
    ap.add_argument("--clock-color", type=rgb, default=(0, 0, 255),
                    help="r,g,b (firmware default 0,0,255)")
    ap.add_argument("--temp-color", type=rgb, default=(255, 0, 0),
                    help="r,g,b (firmware default 255,0,0)")
    ap.add_argument("--plain", action="store_true", help="## and . instead of ANSI color")
    ap.add_argument("--ruler", action="store_true", help="print column numbers (x mod 10)")
    a = ap.parse_args()

    plain = a.plain or "NO_COLOR" in os.environ
    fonts = load_fonts(os.path.join(ROOT, "src", "digits.h"))
    if a.time:
        h, m = (int(v) for v in a.time.split(":"))
    else:
        now = time.localtime()
        h, m = now.tm_hour, now.tm_min
    h = to_12h(h, a.hour_format)
    dots = a.dots == "on"

    views = ["time", "temp", "small", "combined"] if a.view == "all" else [a.view]
    for v in views:
        c = Canvas()
        if v == "time":
            show_time(c, fonts, h, m, dots, a.clock_color)
        elif v == "temp":
            show_temperature(c, fonts, a.temp, a.temp_color)
        elif v == "small":
            show_small(c, fonts, a.temp, a.temp_color)
        else:
            show_combined(c, fonts, h, m, a.temp, dots, a.clock_color, a.temp_color)
        print(f"{v}:")
        print(draw(c, plain, a.ruler))
        print()


if __name__ == "__main__":
    main()
