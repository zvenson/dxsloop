#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The sloopDX logo: SLOOP's dial (the 270-degree arc of the UI's knobs) and its monoline "sloop"
wordmark, with an FM waveform (a sine with a sine in its phase) in the four track colours in place
of the sail, and "DX" in the accent colour after the wordmark.

  tools/gen_logo.py OUT.h          firmware boot splash (RLE, 16-colour palette, RGB565)
  tools/gen_logo.py --assets DIR   sloopdx-logo.svg/png, sloopdx-icon.svg/png, sloopdx-splash.png
"""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

BLUE, GREEN, YELLOW, ORANGE = (0x42, 0xf5, 0xf5), (0xaf, 0xd9, 0xf4), (0xf4, 0xc0, 0xcb), (0xe6, 0xd1, 0xb9)   # sloopDX: cyan, light blue, pink, beige (the DX7's panel)
WHITE, BLACK = (242, 242, 242), (0, 0, 0)
HEX = lambda c: "#%02X%02X%02X" % c
WAVE = [BLUE, GREEN, YELLOW, ORANGE]

# ---- geometry (units: the icon is 240 x 240, the wordmark x-height is 72) -------------------
ICON = dict(r=106, w=12, wave_w=150, wave_h=40, wave_y=-6, stroke=11, base=58, base_y=62)
XH, SW, CAP = 72, 12, 104                        # x-height, stroke, ascender / cap height (l, D, X)


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
    """each letter: (advance width, [("l", x0, y0, x1, y1) | ("a", cx, cy, rx, ry, a0, a1) | ("c", cx, cy, r)]),
    y = 0 at the x-height line, XH at the baseline, negative above (the ascender / caps).
    "sloop" is SLOOP's own wordmark (two bowls, a tall l, circles, a p); "DX" is added in the accent."""
    h, s = XH, SW / 2
    top = XH - CAP
    rxo, ryo = 0.29 * h, (h + SW) / 4                 # the s: two bowls, outer radii
    dw, xw = 0.72 * h, 0.78 * h
    return [
        (2 * rxo, [("a", rxo, ryo, rxo - s, ryo - s, 90, 335), ("a", rxo, h - ryo, rxo - s, ryo - s, 270, 515)]),
        (SW, [("l", s, top + s, s, h - s)]),
        (h, [("c", h / 2, h / 2, h / 2 - s)]),
        (h, [("c", h / 2, h / 2, h / 2 - s)]),
        (h, [("c", h / 2, h / 2, h / 2 - s), ("l", s, h / 2, s, h + DESC - s)]),
        (dw, [("l", s, top + s, s, h - s), ("a", s, (top + h) / 2, dw - 2 * s, (h - top) / 2 - s, 270, 450)]),
        (xw, [("l", s, top + s, xw - s, h - s), ("l", xw - s, top + s, s, h - s)]),
    ]


GAP = 16
DESC = 30                                             # the p's descender
WORD_COLS = [WHITE] * 5 + [BLUE] * 2                  # "sloop" white, "DX" in the accent (cyan)


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
            elif st[0] == "c":
                _, cx, cy, r = st
                cx, cy, r = x + cx * k, oy + cy * k, (r + SW / 2) * k
                d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=col, width=max(1, round(w)))
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
            elif st[0] == "c":
                _, cx, cy, r = st
                s.append(f'<circle cx="{x + cx:.2f}" cy="{oy + cy:.2f}" r="{r:.2f}" fill="none" stroke="{c}" stroke-width="{SW}"/>')
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
            f'aria-label="sloopDX">\n<rect width="{W}" height="{H}" fill="#000"/>\n' + "\n".join(body) + "\n</svg>\n")


# ---- firmware splash ---------------------------------------------------------------------------
SPLASH_W, SPLASH_H = 240, 58
SPLASH_K, SPLASH_KW = 0.54, 0.42


def rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


# the boot splash in the style of the DX7's panel: the dark panel, the wordmark (DX in mint), and the 32
# algorithms as the DX7 prints them above its keys, drawn here from the firmware's own table (dx7_core.c
# DX_ALG): carriers mint on the bottom row, modulators light blue above the operator they feed, feedback orange
DX_PANEL, DX_LABEL, DX_MINT, DX_BLUE, DX_ORANGE = (46, 42, 43), (206, 200, 194), (0x42, 0xf5, 0xf5), (0xaf, 0xd9, 0xf4), (0xf4, 0xc0, 0xcb)


def dx_algs():
    import re
    src = (Path(__file__).resolve().parent.parent / "firmware/src/dx7_core.c").read_text()
    body = src[src.index("DX_ALG[32][6] = {"):]
    body = body[:body.index("};")]
    rows = [[int(x, 16) for x in r.split(",")] for r in re.findall(r"\{(0x[0-9a-fA-F, x]+)\}", body)]
    assert len(rows) == 32
    return rows


def alg_layout(fl):
    """op 0 = OP6 .. 5 = OP1: (x in half columns, depth, columns, carriers, mods, fb in / out), as ui_voice.c"""
    bus, mods, carriers, fbin, fbout = [0, 0, 0], [0] * 6, 0, -1, -1
    for i, f in enumerate(fl):
        inb, outb = (f >> 4) & 3, f & 3
        if inb:
            mods[i] = bus[inb]
        if not outb:
            carriers |= 1 << i
        elif f & 4:
            bus[outb] |= 1 << i
        else:
            bus[outb] = 1 << i
        if f & 0x40:
            fbin = i
        if f & 0x80:
            fbout = i
    tgt = [next((j for j in range(i + 1, 6) if (mods[j] >> i) & 1), -1) for i in range(6)]
    depth = [0] * 6
    for i in range(5, -1, -1):
        depth[i] = 0 if tgt[i] < 0 else depth[tgt[i]] + 1
    x2, ncol = [0] * 6, [0]

    def place(c):
        kids = [j for j in range(5, -1, -1) if tgt[j] == c]
        if kids:
            x2[c] = sum(place(j) for j in kids) / len(kids)
        else:
            x2[c] = 2 * ncol[0] + 1
            ncol[0] += 1
        return x2[c]
    for i in range(5, -1, -1):
        if (carriers >> i) & 1:
            place(i)
    return x2, depth, ncol[0], carriers, mods, fbin, fbout


def draw_alg(d, fl, ox, oy, w, h, s):
    """one algorithm in the box (ox, oy, w, h); s: the supersampling"""
    x2, depth, ncol, carriers, mods, fbin, fbout = alg_layout(fl)
    bw, bh = 4.0 * s, 3.4 * s
    colw = min(w / max(ncol, 1), 5.2 * s)
    rowh = min((h - 3 * s) / (max(depth) + 1), 6.4 * s)
    X = lambda i: ox + w / 2 - ncol * colw / 2 + x2[i] * colw / 2
    Y = lambda i: oy + h - 3 * s - bh / 2 - depth[i] * rowh
    lw = max(1, round(0.7 * s))
    for i in range(6):
        for j in range(6):
            if (mods[j] >> i) & 1:
                d.line([(X(i), Y(i)), (X(j), Y(j))], fill=DX_LABEL, width=lw)
        if (carriers >> i) & 1:
            d.line([(X(i), Y(i)), (X(i), oy + h - 1.2 * s)], fill=DX_LABEL, width=lw)
    d.line([(ox + 1 * s, oy + h - 1.2 * s), (ox + w - 1 * s, oy + h - 1.2 * s)], fill=DX_LABEL, width=lw)
    if fbin >= 0 and fbout >= 0:
        xr = max(X(fbin), X(fbout)) + bw * 0.9
        pts = [(X(fbout) + bw / 2, Y(fbout)), (xr, Y(fbout)), (xr, Y(fbin) - bh * 0.9), (X(fbin), Y(fbin) - bh * 0.9),
               (X(fbin), Y(fbin) - bh / 2)]
        d.line(pts, fill=DX_ORANGE, width=lw)
    for i in range(6):
        c = DX_MINT if (carriers >> i) & 1 else DX_BLUE
        d.rectangle([X(i) - bw / 2, Y(i) - bh / 2, X(i) + bw / 2, Y(i) + bh / 2], fill=c)


def splash_image():
    """the wordmark alone (the algorithms are drawn by the firmware, pixel-sharp: splash.c)"""
    global WORD_COLS
    ss = 4
    im = Image.new("RGB", (SPLASH_W * ss, SPLASH_H * ss), DX_PANEL)
    d = ImageDraw.Draw(im)
    keep = WORD_COLS
    WORD_COLS = [WHITE] * (len(keep) - 2) + [DX_MINT, DX_MINT]   # sloop in white, DX in cyan
    kw = 0.42 * ss
    draw_word(d, (SPLASH_W * ss - word_width(kw)) / 2, 2 * ss + (CAP - XH) * kw, kw)
    WORD_COLS = keep
    return im.resize((SPLASH_W, SPLASH_H), Image.LANCZOS)


def splash_header(path):
    im = splash_image()
    # a fixed palette: black, and each logo colour at 1/3, 2/3 and full (the anti-aliased edges)
    cols = [DX_PANEL]                                 # index 0: the panel (splash.c fills it)
    for c in (WHITE, DX_MINT, DX_BLUE, DX_ORANGE, DX_LABEL):
        cols += [tuple(round(p + (v - p) * f) for v, p in zip(c, DX_PANEL)) for f in (1 / 3, 2 / 3, 1.0)]
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
    L = ["/* generated by tools/gen_logo.py: the sloopDX boot splash */", "#pragma once", "#include <stdint.h>",
         f"#define SLOOPDX_SPLASH_W {SPLASH_W}", f"#define SLOOPDX_SPLASH_H {SPLASH_H}",
         "static const uint16_t SLOOPDX_SPLASH_PAL[16] = {" + ", ".join(f"0x{rgb565(c):04X}" for c in cols) + "};",
         f"static const uint8_t SLOOPDX_SPLASH_RLE[{len(rle)}] = {{"]
    for k in range(0, len(rle), 24):
        L.append("    " + ", ".join(str(b) for b in rle[k:k + 24]) + ",")
    L.append("};")
    Path(path).write_text("\n".join(L) + "\n")
    print(f"logo: splash {SPLASH_W}x{SPLASH_H}, {len(rle)} B RLE -> {path}")


def assets(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "sloopdx-logo.svg").write_text(svg("h"))
    (out / "sloopdx-icon.svg").write_text(svg("i"))
    render("h", 1640, 520, 2.0).save(out / "sloopdx-logo.png")
    render("i", 512, 512, 512 / 240).save(out / "sloopdx-icon.png")
    splash_image().resize((SPLASH_W * 2, SPLASH_H * 2), Image.NEAREST).save(out / "sloopdx-splash.png")
    print(f"logo: assets in {out}")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--assets":
        assets(sys.argv[2])
    elif len(sys.argv) == 2:
        splash_header(sys.argv[1])
    else:
        sys.exit(__doc__)
