#!/usr/bin/env python3
"""
gen_sprites.py - OpenCubeWorld sprite generator.

Draws 16x16 4-bit-palette pixel art for the Cube World characters, whip
effects and dust, then emits:
  1. firmware/tools/../OpenCubeWorld/sprites.h  (PROGMEM sprite tables)
  2. sprites/**/*.png                            (16x upscaled, transparent)

Run from anywhere:  python3 tools/gen_sprites.py
All art is procedural/deterministic so firmware and repo PNGs always match.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# ---------------------------------------------------------------- palette --
# 16-colour palette shared by every sprite (565 values computed from RGB).
def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 6)

PALETTE_RGB = [
    (0, 0, 0),       # 0  transparent
    (20, 16, 32),    # 1  ink (outline, pupils, mouth)
    (255, 255, 255), # 2  white (eyes)
    (120, 128, 140), # 3  mid gray (dust)
    (51, 170, 85),   # 4  green body   (Open)
    (226, 60, 54),   # 5  red body     (Whip)
    (58, 110, 224),  # 6  blue body    (Dodger)
    (235, 140, 42),  # 7  orange body  (Hans)
    (150, 84, 214),  # 8  purple body  (Handy)
    (240, 200, 60),  # 9  yellow       (spare)
    (24, 110, 56),   # 10 dark green
    (140, 34, 30),   # 11 dark red
    (34, 70, 150),   # 12 dark blue
    (160, 92, 24),   # 13 dark orange
    (96, 50, 140),   # 14 dark purple
    (220, 228, 235), # 15 highlight
]
PALETTE_565 = [rgb565(*c) for c in PALETTE_RGB]

# --------------------------------------------------------------- canvas ----
W = H = 16

class Canvas:
    def __init__(self):
        self.px = [[0] * W for _ in range(H)]

    def put(self, x, y, c):
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.put(x, y, c)

    def hline(self, x0, x1, y, c):
        self.rect(x0, y, x1, y, c)

    def rounded_rect(self, x0, y0, x1, y1, c, r=1):
        self.rect(x0 + r, y0, x1 - r, y1, c)
        self.rect(x0, y0 + r, x1, y1 - r, c)
        self.put(x0 + r, y0, c); self.put(x1 - r, y0, c)
        self.put(x0 + r, y1, c); self.put(x1 - r, y1, c)

# ---------------------------------------------------------- characters -----
# Each character: (name, body, shade, hat, pupil_dx)
CHARACTERS = [
    dict(name="open",   body=4, shade=10, hat="antenna",  pupil_dx=0),
    dict(name="whip",   body=5, shade=11, hat="zigzag",   pupil_dx=1),
    dict(name="dodger", body=6, shade=12, hat="nubs",     pupil_dx=1),
    dict(name="hans",   body=7, shade=13, hat="cap",      pupil_dx=0),
    dict(name="handy",  body=8, shade=14, hat="bow",      pupil_dx=-1),
]

INK, WHITE, HILITE = 1, 2, 15

def draw_hat(cv, kind, body, shade, dy=0):
    """Small personality marker on the crown of the cube (rows 1-2)."""
    y = 2 + dy
    if kind == "antenna":
        cv.put(7, 1 + dy, shade); cv.put(8, 1 + dy, shade)
        cv.put(8, 2 + dy, body)
    elif kind == "zigzag":
        cv.put(5, 2 + dy, shade); cv.put(7, 1 + dy, shade)
        cv.put(9, 2 + dy, shade); cv.put(11, 1 + dy, shade)
    elif kind == "nubs":
        cv.rect(4, 1 + dy, 5, 2 + dy, shade)
        cv.rect(10, 1 + dy, 11, 2 + dy, shade)
    elif kind == "cap":
        cv.hline(5, 10, 2 + dy, shade)
        cv.hline(5, 10, 1 + dy, body)
    elif kind == "bow":
        cv.put(6, 2 + dy, shade); cv.put(9, 2 + dy, shade)
        cv.put(7, 1 + dy, shade); cv.put(8, 1 + dy, shade)

def draw_body(cv, body, shade, dy=0):
    cv.rounded_rect(2, 3 + dy, 13, 14, body)
    cv.hline(3, 5, 4 + dy, HILITE)          # top highlight
    cv.hline(3, 13, 13, shade)              # bottom shade
    cv.vline = lambda: None
    for y in range(5 + dy, 13):
        cv.put(2, y, shade); cv.put(13, y, shade)

def draw_eyes(cv, mode, dx=0):
    # big cartoon eyes: white 3x3, 1x1 pupil
    for ex in (4, 9):
        cv.rect(ex, 6, ex + 2, 8, WHITE)
        if mode == "idle":
            cv.put(ex + 1 + dx, 7, INK)
        elif mode == "squint":
            cv.rect(ex, 7, ex + 2, 7, INK)
        elif mode == "wide":
            cv.put(ex + 1 + dx, 7, INK)
            cv.put(ex + 1 + dx, 8, INK)
        elif mode == "x":
            cv.put(ex, 6, INK); cv.put(ex + 2, 8, INK)
            cv.put(ex + 2, 6, INK); cv.put(ex, 8, INK)
        elif mode == "happy":
            cv.rect(ex, 7, ex + 2, 8, INK)

def draw_mouth(cv, mode):
    if mode == "smile":
        cv.put(6, 11, INK); cv.put(7, 12, INK)
        cv.put(8, 12, INK); cv.put(9, 11, INK)
    elif mode == "open":
        cv.rect(6, 11, 9, 13, INK)
        cv.rect(7, 12, 8, 12, WHITE)
    elif mode == "big":
        cv.rect(5, 11, 10, 13, INK)
        cv.rect(6, 12, 9, 13, WHITE)
    elif mode == "o":
        cv.rect(7, 11, 8, 12, INK)

def draw_char(char, variant):
    """variant: idle0 idle1 whip dodge wave sit"""
    cv = Canvas()
    body, shade, hat, pdx = char["body"], char["shade"], char["hat"], char["pupil_dx"]
    dy = 0
    if variant == "idle1":
        dy = 1
    if variant == "sit":
        dy = 2
    if variant == "dodge":
        dy = 1
    draw_hat(cv, hat, body, shade, dy=dy)
    draw_body(cv, body, shade, dy=dy)
    if variant == "idle0":
        draw_eyes(cv, "idle", pdx)
        draw_mouth(cv, "smile")
    elif variant == "idle1":
        draw_eyes(cv, "squint", pdx)
        draw_mouth(cv, "smile")
    elif variant == "whip":
        draw_eyes(cv, "wide", pdx)
        draw_mouth(cv, "open")
    elif variant == "dodge":
        draw_eyes(cv, "x", pdx)
        draw_mouth(cv, "o")
    elif variant == "wave":
        draw_eyes(cv, "happy", pdx)
        draw_mouth(cv, "big")
    elif variant == "sit":
        draw_eyes(cv, "squint", pdx)
        draw_mouth(cv, "o")
    if variant == "wave":
        cv.put(13, 3, body); cv.put(13, 2, body)   # raised nub (waving)
    return cv

# ------------------------------------------------------------------ fx -----
def draw_whip_frame(f):
    cv = Canvas()
    if f == 0:
        for i in range(4):
            cv.put(2 + i, 2 + i, WHITE); cv.put(3 + i, 3 + i, 2)
    elif f == 1:
        for i in range(8):
            cv.put(2 + i, 2 + i, WHITE); cv.put(3 + i, 3 + i, 9)
            if 2 <= i < 7:
                cv.put(1 + i, 1 + i, WHITE)
    else:
        for i in range(8):
            cv.put(2 + i, 2 + i, WHITE)
        cv.put(10, 10, 9); cv.put(11, 10, 9); cv.put(12, 10, 9)
        cv.put(10, 11, 9); cv.put(10, 12, 9)
        cv.put(12, 12, 2)
    return cv

def draw_dust(f):
    cv = Canvas()
    if f == 0:
        cv.put(5, 12, 3); cv.put(7, 11, 3); cv.put(9, 12, 3)
        cv.put(6, 13, 3); cv.put(8, 13, 3); cv.put(10, 13, 3)
    else:
        cv.put(4, 10, 3); cv.put(7, 9, 3); cv.put(10, 10, 3)
        cv.put(5, 11, 3); cv.put(8, 11, 3); cv.put(11, 11, 3)
    return cv

# ------------------------------------------------------------- emit -------
VARIANTS = ["idle0", "idle1", "whip", "dodge", "wave", "sit"]
FRAME = {v: i for i, v in enumerate(VARIANTS)}

def to_bytes(cv):
    out = bytearray(128)
    for y in range(H):
        for x in range(W):
            out[(y * W + x) // 2] |= (cv.px[y][x] & 0xF) << (4 if (x & 1) == 0 else 0)
    return bytes(out)

def png_save(cv, path, scale=16):
    try:
        from PIL import Image
    except ImportError:
        return
    img = Image.new("RGBA", (W * scale, H * scale), (0, 0, 0, 0))
    for y in range(H):
        for x in range(W):
            idx = cv.px[y][x]
            if idx == 0:
                continue
            r, g, b = PALETTE_RGB[idx]
            img.putpixel((x * scale, y * scale), (r, g, b, 255))
    img.resize((W * scale, H * scale), Image.NEAREST)
    img.save(path)

def emit_header(chars, whip_frames, dust_frames, path):
    L = []
    L.append("// Auto-generated by tools/gen_sprites.py - DO NOT EDIT BY HAND.")
    L.append("// 16x16 sprites, 4-bit palette, MSB-first, row-major.")
    L.append("#pragma once")
    L.append("#include <Arduino.h>")
    L.append("")
    L.append("static const uint16_t OCW_PALETTE[16] PROGMEM = {")
    L.append("  " + ", ".join("0x%04X" % v for v in PALETTE_565) + ",")
    L.append("};")
    L.append("")
    L.append("static const char *const OCW_CHAR_NAMES[] PROGMEM = { " +
             ", ".join('"%s"' % c["name"] for c in chars) + " };")
    L.append("")
    L.append("// frame indices")
    for v in VARIANTS:
        L.append("#define OCW_FR_%s %d" % (v.upper(), FRAME[v]))
    L.append("#define OCW_NFRAMES %d" % len(VARIANTS))
    L.append("")
    for c in chars:
        L.append("static const uint8_t ocw_%s[6][128] PROGMEM = {" % c["name"])
        for v in VARIANTS:
            b = to_bytes(draw_char(c, v))
            L.append("  {" + ", ".join(str(x) for x in b) + "},  // %s" % v)
        L.append("};")
        L.append("")
    L.append("static const uint8_t ocw_fx_whip[3][128] PROGMEM = {")
    for f in whip_frames:
        b = to_bytes(f)
        L.append("  {" + ", ".join(str(x) for x in b) + "},")
    L.append("};")
    L.append("")
    L.append("static const uint8_t ocw_fx_dust[2][128] PROGMEM = {")
    for f in dust_frames:
        b = to_bytes(f)
        L.append("  {" + ", ".join(str(x) for x in b) + "},")
    L.append("};")
    with open(path, "w") as f:
        f.write("\n".join(L) + "\n")

def main():
    chars = []
    os.makedirs(os.path.join(ROOT, "sprites", "characters"), exist_ok=True)
    for c in CHARACTERS:
        canvases = {v: draw_char(c, v) for v in VARIANTS}
        chars.append(canvases)
        base = os.path.join(ROOT, "sprites", "characters")
        for v in VARIANTS:
            png_save(canvases[v], os.path.join(base, "%s_%s.png" % (c["name"], v)))
    whip = [draw_whip_frame(f) for f in range(3)]
    dust = [draw_dust(f) for f in range(2)]
    os.makedirs(os.path.join(ROOT, "sprites", "fx"), exist_ok=True)
    for f, cv in enumerate(whip):
        png_save(cv, os.path.join(ROOT, "sprites", "fx", "whip_%d.png" % f))
    for f, cv in enumerate(dust):
        png_save(cv, os.path.join(ROOT, "sprites", "fx", "dust_%d.png" % f))
    emit_header(CHARACTERS, whip, dust, os.path.join(ROOT, "firmware", "libraries", "OpenCubeWorld", "sprites.h"))
    print("sprites.h written; %d characters x %d frames + fx" % (len(chars), len(VARIANTS)))

if __name__ == "__main__":
    main()
