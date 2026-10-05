#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The zvenFM logo: an FM waveform (a sine with a sine in its phase) in the four track colours
inside the 270-degree dial of the UI's knobs, and a geometric monoline wordmark "zvenFM".
(The dial and the monoline wordmark keep SLOOP's logo language, which this firmware grew from.)

  tools/gen_logo.py OUT.h          firmware boot splash (RLE, 16-colour palette, RGB565)
  tools/gen_logo.py --assets DIR   zvenfm-logo.svg/png, zvenfm-icon.svg/png, zvenfm-splash.png
"""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

BLUE, GREEN, YELLOW, ORANGE = (40, 124, 255), (30, 204, 112), (255, 198, 24), (255, 98, 26)
WHITE, BLACK = (242, 242, 242), (0, 0, 0)
HEX = lambda c: "#%02X%02X%02X" % c
WAVE = [BLUE, GREEN, YELLOW, ORANGE]

# ---- geometry (units: the icon is 240 x 240, the wordmark x-height is 72) -------------------
ICON = dict(r=106, w=12, wave_w=150, wave_h=40, wave_y=-6, stroke=11, base=58, base_y=62)
XH, SW, CAP = 72, 12, 104                        # x-height, stroke, cap height (F, M)


def wave_points(cx, cy, k, n=240):
    """y = sin(t + I(t) sin(2t)) over two periods; the index I rises left to right: FM in one line"""
    g = ICON
    w, h, y0 = g["wave_w"] * k, g["wave_h"] * k, cy + g["wave_y"] * k
    pts = []
    for i in range(n + 1):
        u = i / n
        t = u * 4 * math.pi
        idx = 1.5 * u * u
        pts.append((cx - w / 2 + u * w, y0 - h * math.sin(t + idx * math.sin(2 * t))))
    return pts


def wave_segments(cx, cy, k):
    pts = wave_points(cx, cy, k)
    n = len(pts) - 1
    return [(c, pts[i * n // 4: (i + 1) * n // 4 + 1]) for i, c in enumerate(WAVE)]


def rrect(d, box, r, fill):
    """rounded rectangle (ImageDraw.rounded_rectangle needs Pillow 8.2)"""
    x0, y0, x1, y1 = box
    r = min(r, (x1 - x0) / 2, (y1 - y0) / 2)
    d.rectangle([x0 + r, y0, x1 - r, y1], fill=fill)
    d.rectangle([x0, y0 + r, x1, y1 - r], fill=fill)
    for cx, cy in ((x0 + r, y0 + r), (x1 - r, y0 + r), (x0 + r, y1 - r), (x1 - r, y1 - r)):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=fill)


def poly_line(d, pts, col, w):
    """a round-joined thick polyline (PIL's joint="curve" leaves gaps at small scales)"""
    d.line(pts, fill=col, width=max(1, round(w)))
    for x, y in pts[:: max(1, len(pts) // 60)] + [pts[-1]]:
        d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=col)


def draw_icon(d, ox, oy, k):
    g = ICON
    cx, cy, r, w = ox + 120 * k, oy + 120 * k, g["r"] * k, g["w"] * k
    d.arc([cx - r, cy - r, cx + r, cy + r], start=135, end=405, fill=WHITE, width=max(1, round(w)))
    for a in (135, 405):
        x = cx + (r - w / 2) * math.cos(math.radians(a))
        y = cy + (r - w / 2) * math.sin(math.radians(a))
        d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=WHITE)
    for c, pts in wave_segments(cx, cy, k):
        poly_line(d, pts, c, g["stroke"] * k)
    rrect(d, [cx - g["base"] * k, cy + g["base_y"] * k, cx + g["base"] * k, cy + (g["base_y"] + 12) * k], 6 * k, WHITE)


# ---- the wordmark: z v e n F M, monoline strokes with round caps --------------------------------
def glyphs():
    """each letter: (advance width, [("l", x0, y0, x1, y1) | ("a", cx, cy, rx, ry, a0, a1)]),
    y = 0 at the x-height line, XH at the baseline, negative above (the caps)"""
    h, s = XH, SW / 2
    zw, vw, nw, fw, mw = 0.62 * h, 0.70 * h, 0.66 * h, 0.58 * h, 0.92 * h
    top = XH - CAP
    return [
        (zw, [("l", s, s, zw - s, s), ("l", zw - s, s, s, h - s), ("l", s, h - s, zw - s, h - s)]),
        (vw, [("l", s, s, vw / 2, h - s), ("l", vw / 2, h - s, vw - s, s)]),
        (h, [("a", h / 2, h / 2, h / 2 - s, h / 2 - s, 40, 360), ("l", s, h / 2, h - s, h / 2)]),
        (nw, [("l", s, s, s, h - s), ("a", nw / 2, nw / 2, nw / 2 - s, nw / 2 - s, 180, 360),
              ("l", nw - s, nw / 2, nw - s, h - s)]),
        (fw, [("l", s, top + s, s, h - s), ("l", s, top + s, fw - s, top + s), ("l", s, (top + h) / 2, fw - 0.2 * h, (top + h) / 2)]),
        (mw, [("l", s, h - s, s, top + s), ("l", s, top + s, mw / 2, (top + h) / 2 + 6), ("l", mw / 2, (top + h) / 2 + 6, mw - s, top + s),
              ("l", mw - s, top + s, mw - s, h - s)]),
    ]


GAP = 16
WORD_COLS = [WHITE, WHITE, WHITE, WHITE, ORANGE, ORANGE]   # "zven" white, "FM" in the accent


def word_width(k):
    gl = glyphs()
    return (sum(g[0] for g in gl) + GAP * (len(gl) - 1)) * k


def draw_word(d, ox, oy, k):
    w = SW * k
    x = ox
    for (adv, strokes), col in zip(glyphs(), WORD_COLS):
        def cap(px, py):
            d.ellipse([px - w / 2, py - w / 2, px + w / 2, py + w / 2], fill=col)
        for st in strokes:
            if st[0] == "l":
                _, x0, y0, x1, y1 = st
                p0, p1 = (x + x0 * k, oy + y0 * k), (x + x1 * k, oy + y1 * k)
                d.line([p0, p1], fill=col, width=max(1, round(w)))
                cap(*p0)
                cap(*p1)
            else:
                _, cx, cy, rx, ry, a0, a1 = st
                cx, cy, rx, ry = x + cx * k, oy + cy * k, (rx + SW / 2) * k, (ry + SW / 2) * k
                d.arc([cx - rx, cy - ry, cx + rx, cy + ry], start=a0, end=a1, fill=col, width=max(1, round(w)))
                for a in (a0, a1):
                    cap(cx + (rx - w / 2) * math.cos(math.radians(a)), cy + (ry - w / 2) * math.sin(math.radians(a)))
        x += (adv + GAP) * k


def render(kind, W, H, k, bg=BLACK, ss=4, kw=None):
    im = Image.new("RGB", (W * ss, H * ss), bg)
    d = ImageDraw.Draw(im)
    K = k * ss
    KW = (kw or k) * ss
    if kind == "h":                                   # icon + wordmark side by side
        draw_icon(d, 10 * K, (H * ss - 240 * K) / 2, K)
        draw_word(d, 280 * K, H * ss / 2 - 30 * K, KW)
    elif kind == "v":                                 # icon over the wordmark (the boot splash)
        draw_icon(d, (W * ss - 240 * K) / 2, 0, K)
        draw_word(d, (W * ss - word_width(KW)) / 2, 236 * K + (CAP - XH) * KW, KW)
    else:
        draw_icon(d, (W * ss - 240 * K) / 2, (H * ss - 240 * K) / 2, K)
    return im.resize((W, H), Image.LANCZOS)


# ---- SVG (stroke centred on the path) ---------------------------------------------------------
def svg_icon(ox, oy):
    g = ICON
    cx, cy, w = ox + 120, oy + 120, g["w"]
    r = g["r"] - w / 2
    a0, a1 = math.radians(135), math.radians(405)
    p0 = (cx + r * math.cos(a0), cy + r * math.sin(a0))
    p1 = (cx + r * math.cos(a1), cy + r * math.sin(a1))
    s = [f'<path d="M{p0[0]:.2f} {p0[1]:.2f} A{r:.2f} {r:.2f} 0 1 1 {p1[0]:.2f} {p1[1]:.2f}" fill="none" '
         f'stroke="{HEX(WHITE)}" stroke-width="{w}" stroke-linecap="round"/>']
    for c, pts in wave_segments(cx, cy, 1.0):
        s.append('<polyline points="' + " ".join(f"{x:.2f},{y:.2f}" for x, y in pts) +
                 f'" fill="none" stroke="{HEX(c)}" stroke-width="{g["stroke"]}" stroke-linecap="round" stroke-linejoin="round"/>')
    s.append(f'<rect x="{cx - g["base"]:.2f}" y="{cy + g["base_y"]:.2f}" width="{2 * g["base"]}" height="12" rx="6" fill="{HEX(WHITE)}"/>')
    return s


def svg_word(ox, oy):
    s, x = [], ox
    for (adv, strokes), col in zip(glyphs(), WORD_COLS):
        c = HEX(col)
        for st in strokes:
            if st[0] == "l":
                _, x0, y0, x1, y1 = st
                s.append(f'<line x1="{x + x0:.2f}" y1="{oy + y0:.2f}" x2="{x + x1:.2f}" y2="{oy + y1:.2f}" '
                         f'stroke="{c}" stroke-width="{SW}" stroke-linecap="round"/>')
            else:
                _, cx, cy, rx, ry, a0, a1 = st
                cx, cy = x + cx, oy + cy
                p = (cx + rx * math.cos(math.radians(a0)), cy + ry * math.sin(math.radians(a0)))
                q = (cx + rx * math.cos(math.radians(a1)), cy + ry * math.sin(math.radians(a1)))
                large = 1 if (a1 - a0) > 180 else 0
                s.append(f'<path d="M{p[0]:.2f} {p[1]:.2f} A{rx:.2f} {ry:.2f} 0 {large} 1 {q[0]:.2f} {q[1]:.2f}" '
                         f'fill="none" stroke="{c}" stroke-width="{SW}" stroke-linecap="round"/>')
        x += adv + GAP
    return s


def svg(kind):
    if kind == "h":
        W, H = 820, 260
        body = svg_icon(10, 10) + svg_word(280, H / 2 - 30)
    else:
        W, H = 240, 240
        body = svg_icon(0, 0)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" '
            f'aria-label="zvenFM">\n<rect width="{W}" height="{H}" fill="#000"/>\n' + "\n".join(body) + "\n</svg>\n")


# ---- firmware splash ---------------------------------------------------------------------------
SPLASH_W, SPLASH_H = 240, 188
SPLASH_K, SPLASH_KW = 0.56, 0.44


def rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def splash_image():
    return render("v", SPLASH_W, SPLASH_H, SPLASH_K, kw=SPLASH_KW)


def splash_header(path):
    im = splash_image()
    # a fixed palette: black, and each logo colour at 1/3, 2/3 and full (the anti-aliased edges)
    cols = [BLACK]
    for c in (WHITE, BLUE, GREEN, YELLOW, ORANGE):
        cols += [tuple(round(v * f) for v in c) for f in (1 / 3, 2 / 3, 1.0)]
    src = im.tobytes()
    px = []
    cache = {}
    for i in range(0, len(src), 3):
        p = (src[i], src[i + 1], src[i + 2])
        if p not in cache:
            cache[p] = min(range(16), key=lambda k: sum((p[j] - cols[k][j]) ** 2 for j in range(3)))
        px.append(cache[p])
    rle = bytearray()                                 # (run - 1) << 4 | index, runs of 1..16
    i = 0
    while i < len(px):
        j = i
        while j < len(px) and px[j] == px[i] and j - i < 16:
            j += 1
        rle.append(((j - i - 1) << 4) | px[i])
        i = j
    L = ["/* generated by tools/gen_logo.py: the zvenFM boot splash */", "#pragma once", "#include <stdint.h>",
         f"#define ZVEN_SPLASH_W {SPLASH_W}", f"#define ZVEN_SPLASH_H {SPLASH_H}",
         "static const uint16_t ZVEN_SPLASH_PAL[16] = {" + ", ".join(f"0x{rgb565(c):04X}" for c in cols) + "};",
         f"static const uint8_t ZVEN_SPLASH_RLE[{len(rle)}] = {{"]
    for k in range(0, len(rle), 24):
        L.append("    " + ", ".join(str(b) for b in rle[k:k + 24]) + ",")
    L.append("};")
    Path(path).write_text("\n".join(L) + "\n")
    print(f"logo: splash {SPLASH_W}x{SPLASH_H}, {len(rle)} B RLE -> {path}")


def assets(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "zvenfm-logo.svg").write_text(svg("h"))
    (out / "zvenfm-icon.svg").write_text(svg("i"))
    render("h", 1640, 520, 2.0).save(out / "zvenfm-logo.png")
    render("i", 512, 512, 512 / 240).save(out / "zvenfm-icon.png")
    splash_image().resize((SPLASH_W * 2, SPLASH_H * 2), Image.NEAREST).save(out / "zvenfm-splash.png")
    print(f"logo: assets in {out}")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--assets":
        assets(sys.argv[2])
    elif len(sys.argv) == 2:
        splash_header(sys.argv[1])
    else:
        sys.exit(__doc__)
