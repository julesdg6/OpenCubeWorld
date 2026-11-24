#!/usr/bin/env python3
"""gen_images.py - render OpenCubeWorld repo images from firmware data.

Reads the 4bpp 16x16 frame tables + palette from
firmware/libraries/OpenCubeWorld/sprites.h and renders:

  assets/sheets/<char>.svg      all 6 frames of a character, 8x scale
  assets/banner.svg             repo banner with the 5 idle characters
  assets/screenshot.svg         a 240x240 simulation of the main screen

Plus PNG twins (PIL required):
  assets/sheets/<char>_sheet.png, assets/banner.png, assets/screenshot.png
  sprites/characters/<char>/<state>.png   raw 16x16 frames
  sprites/fx/*.png                        raw 16x16 frames

Usage:  python3 tools/gen_images.py [repo_root]
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(HERE)

FRAME_STATES = ["idle0", "idle1", "sit", "dodge", "wave", "whip"]
CHARS = ["open", "whip", "dodger", "hans", "handy"]
CHAR_LABELS = {
    "open": "Open (0x01)", "whip": "Whip (0x06)", "dodger": "Dodger (0x0E)",
    "hans": "Hans (0x0D)", "handy": "Handy (0x0B)",
}
SCREEN_COLORS = {  # RGB565 constants from world.h
    "bg": 0x0841, "bar": 0x1298, "text": 0xE73F, "dim": 0x64A2,
    "ground": 0x3E0F, "grass": 0x2E8F, "accent": 0xF640,
}


def parse_sprites_h(path):
    src = open(path).read()
    m = re.search(r"OCW_PALETTE\[16\]\s*PROGMEM\s*=\s*\{(.*?)\};", src, re.S)
    pal565 = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{4})", m.group(1))]

    def table(name, rows, cols):
        m = re.search(r"ocw_%s\s*\[%d\]\s*\[%d\]\s*PROGMEM\s*=\s*\{(.*?)\};"
                      % (name, rows, cols), src, re.S)
        return [[int(x) for x in t.split(",") if x.strip()]
                for t in re.findall(r"\{(.*?)\}", m.group(1))]

    return pal565, {c: table(c, 6, 128) for c in CHARS}, \
        table("fx_whip", 3, 128), table("fx_dust", 2, 128)


def to_pixels(frame):
    px = []
    for y in range(16):
        row = []
        for x in range(16):
            k = (y * 16 + x) // 2
            row.append(frame[k] & 0x0F if (x & 1) else (frame[k] >> 4))
        px.append(row)
    return px


def rgb565_to_hex(v):
    r = (v >> 11) & 0x1F
    g = (v >> 5) & 0x3F
    b = v & 0x1F
    return "#%02X%02X%02X" % (((r << 3) | (r >> 2)), ((g << 2) | (g >> 1)),
                              ((b << 3) | (b >> 2)))


def rgb565_to_argb8888(pal565, idx):
    v = pal565[idx]
    r = (v >> 11) & 0x1F
    g = (v >> 5) & 0x3F
    b = v & 0x1F
    return ((r << 3) | (r >> 2), ((g << 2) | (g >> 1)), ((b << 3) | (b >> 2)), 255)


def frame_defs(frames, pal_hex, prefix):
    out = []
    for i, fr in enumerate(frames):
        rects = []
        for y in range(16):
            x = 0
            while x < 16:
                idx = to_pixels(fr)[y][x]
                if idx == 0:
                    x += 1
                    continue
                x2 = x
                while x2 < 16 and to_pixels(fr)[y][x2] == idx:
                    x2 += 1
                rects.append('<rect x="%d" y="%d" width="%d" height="1" fill="%s"/>'
                             % (x, y, x2 - x, pal_hex[idx]))
                x = x2
        out.append('    <g id="%s-f%d">\n      %s\n    </g>'
                   % (prefix, i, "\n      ".join(rects)))
    return "\n".join(out)


def svg_header(w, h, bg):
    return ('<svg xmlns="http://www.w3.org/2000/svg" '
            'xmlns:xlink="http://www.w3.org/1999/xlink" '
            'width="%d" height="%d" viewBox="0 0 %d %d">\n'
            '  <rect width="100%%" height="100%%" fill="%s"/>\n'
            % (w, h, w, h, bg))


def make_sheet(char, frames, pal_hex, col):
    scale = 8
    pad = 12
    w = 6 * 16 * scale + 7 * pad
    h = 16 * scale + pad * 2 + 22
    body = []
    for i in range(6):
        x = pad + i * (16 * scale + pad)
        body.append('<use xlink:href="#%s-f%d" transform="translate(%d,%d) scale(%d)"/>'
                    % (char, i, x, pad, scale))
        body.append('<text x="%d" y="%d" font-family="monospace" font-size="11" '
                    'fill="%s" text-anchor="middle">%s</text>'
                    % (x + 8 * scale, pad + 16 * scale + 14, col["dim"], FRAME_STATES[i]))
    return (svg_header(w, h, "#0B1026")
            + '  <text x="%d" y="22" font-family="monospace" font-size="15" fill="%s">%s</text>\n'
            % (pad, col["text"], CHAR_LABELS[char])
            + "  <defs>\n" + frame_defs(frames, pal_hex, char) + "\n  </defs>\n  "
            + "\n  ".join(body) + "\n</svg>\n")


def make_banner(chars, pal_hex, col):
    scale = 10
    n = len(CHARS)
    w = n * 16 * scale + (n + 1) * 34
    h = 16 * scale + 175
    body = []
    for i, c in enumerate(CHARS):
        x = 34 + i * (16 * scale + 34)
        body.append('<use xlink:href="#b-%s-f0" transform="translate(%d,105) scale(%d)"/>'
                    % (c, x, scale))
        body.append('<text x="%d" y="%d" font-family="monospace" font-size="15" '
                    'fill="%s" text-anchor="middle">%s</text>'
                    % (x + 8 * scale, 105 + 16 * scale + 20, col["dim"], CHAR_LABELS[c]))
    # pulse-train motif (a real packet's shape)
    x = 34
    for d, hi in [(24, 1), (8, 0), (12, 1), (8, 0), (20, 1), (8, 0), (12, 1),
                  (8, 0), (12, 1), (8, 0), (12, 1), (8, 0), (12, 1), (8, 0), (20, 1)]:
        body.append('<rect x="%d" y="%d" width="%d" height="9" fill="%s"/>'
                    % (x, 58 if hi else 64, d, col["text"] if hi else col["bar"]))
        x += d + 3
    subtitle = ('  <text x="34" y="98" font-family="monospace" font-size="15" '
                'fill="%s">Cube World toys, reborn on the Waveshare 1.54" Touch '
                'LCD (SKU 34370)</text>\n' % col["dim"])
    return (svg_header(w, h, "#0B1026")
            + '  <rect x="0" y="0" width="100%%" height="6" fill="%s"/>\n' % col["accent"]
            + '  <text x="34" y="46" font-family="monospace" font-size="36" font-weight="bold" '
            + 'fill="%s">OPEN CUBE WORLD</text>\n' % col["accent"]
            + subtitle
            + "  <defs>\n"
            + "\n".join(frame_defs([chars[c][0]], pal_hex, "b-%s" % c) for c in CHARS)
            + "\n  </defs>\n  " + "\n  ".join(body) + "\n</svg>\n")


def make_screenshot(chars, pal_hex, fx_whip, col):
    W = H = 240
    scale = 6
    ground_y = 196
    cubes = [("open", 28, 0), ("dodger", 100, 5), ("hans", 172, 0)]
    body = []
    for c, x, fi in cubes:
        y = ground_y - 16 * scale
        body.append('<use xlink:href="#s-%s" transform="translate(%d,%d) scale(%d)"/>'
                    % (c, x, y, scale))
    for c, x, fi in cubes:
        body.append('<ellipse cx="%d" cy="%d" rx="%d" ry="4" fill="%s"/>'
                    % (x + 8 * scale, ground_y + 5, 7 * scale, col["dim"]))
    # whip arc fx (3x) between Open and Dodger
    fx = to_pixels(fx_whip[1])
    for y in range(16):
        x = 0
        while x < 16:
            idx = fx[y][x]
            if idx == 0:
                x += 1
                continue
            x2 = x
            while x2 < 16 and fx[y][x2] == idx:
                x2 += 1
            body.append('<rect x="%d" y="%d" width="%d" height="%d" fill="%s"/>'
                        % (86 + x * 3, 128 + y * 3, (x2 - x) * 3, 3, pal_hex[idx]))
            x = x2
    body.append('<text x="120" y="188" font-family="monospace" font-size="9" '
                'fill="%s" text-anchor="middle">1 whip - Dodger dodges</text>' % col["dim"])
    return (svg_header(W, H, col["bg"])
            + '  <rect x="0" y="0" width="%d" height="14" fill="%s"/>\n' % (W, col["bar"])
            + '  <text x="4" y="11" font-family="monospace" font-size="10" fill="%s">OPEN CUBE WORLD</text>\n'
            % col["text"]
            + '  <text x="%d" y="11" font-family="monospace" font-size="10" fill="%s" '
            'text-anchor="end">LIVE</text>\n' % (W - 4, col["accent"])
            + '  <rect x="0" y="%d" width="%d" height="8" fill="%s"/>\n'
            '  <rect x="0" y="%d" width="%d" height="2" fill="%s"/>\n'
            % (ground_y, W, col["ground"], ground_y, W, col["grass"])
            + "  <defs>\n"
            + "\n".join(frame_defs([chars[c][fi]], pal_hex, "s-%s" % c) for c, x, fi in cubes)
            + "\n  </defs>\n  " + "\n  ".join(body) + "\n</svg>\n")


def write_png_frames():
    try:
        from PIL import Image
    except ImportError:
        print("PIL not available - skipping PNG twins")
        return
    pal565 = _pal565
    cdir = os.path.join(ROOT, "sprites/characters")
    for c in CHARS:
        d = os.path.join(cdir, c)
        os.makedirs(d, exist_ok=True)
        for i, st in enumerate(FRAME_STATES):
            im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
            px = im.load()
            for y in range(16):
                for x in range(16):
                    idx = to_pixels(_chars[c][i])[y][x]
                    if idx:
                        px[x, y] = rgb565_to_argb8888(pal565, idx)
            im.save(os.path.join(d, st + ".png"))
    fdir = os.path.join(ROOT, "sprites/fx")
    os.makedirs(fdir, exist_ok=True)
    for name, frames in (("whip", _fx_whip), ("dust", _fx_dust)):
        for i, fr in enumerate(frames):
            im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
            px = im.load()
            for y in range(16):
                for x in range(16):
                    idx = to_pixels(fr)[y][x]
                    if idx:
                        px[x, y] = rgb565_to_argb8888(pal565, idx)
            im.save(os.path.join(fdir, "%s_%d.png" % (name, i)))
    # 8x sheet PNGs
    for c in CHARS:
        im = Image.new("RGBA", (6 * 16 * 8 + 7 * 12, 16 * 8 + 24), (11, 16, 38, 255))
        for i in range(6):
            fr = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
            fpx = fr.load()
            for y in range(16):
                for x in range(16):
                    idx = to_pixels(_chars[c][i])[y][x]
                    if idx:
                        fpx[x, y] = rgb565_to_argb8888(pal565, idx)
            fr = fr.resize((16 * 8, 16 * 8), Image.NEAREST)
            im.paste(fr, (12 + i * (16 * 8 + 12), 12), fr)
        im.save(os.path.join(ROOT, "assets/sheets", c + "_sheet.png"))
    print("wrote raw 16x16 PNGs + 8x sheet PNGs")


_pal565 = _chars = _fx_whip = _fx_dust = None


def main():
    global _pal565, _chars, _fx_whip, _fx_dust
    _pal565, _chars, _fx_whip, _fx_dust = parse_sprites_h(
        os.path.join(ROOT, "firmware/libraries/OpenCubeWorld/sprites.h"))
    pal_hex = [rgb565_to_hex(v) for v in _pal565]
    col = {k: rgb565_to_hex(v) for k, v in SCREEN_COLORS.items()}

    os.makedirs(os.path.join(ROOT, "assets/sheets"), exist_ok=True)
    for c in CHARS:
        open(os.path.join(ROOT, "assets/sheets", c + ".svg"), "w").write(
            make_sheet(c, _chars[c], pal_hex, col))
    open(os.path.join(ROOT, "assets/banner.svg"), "w").write(
        make_banner(_chars, pal_hex, col))
    open(os.path.join(ROOT, "assets/screenshot.svg"), "w").write(
        make_screenshot(_chars, pal_hex, _fx_whip, col))
    for f in ["banner.svg", "screenshot.svg"]:
        print("wrote assets/" + f)
    for c in CHARS:
        print("wrote assets/sheets/" + c + ".svg")
    write_png_frames()


if __name__ == "__main__":
    main()
