#!/usr/bin/env python3
"""
gen_images.py - Render authentic monochrome 32x32 Radica Cube World SVG/PNG repo assets.

Reads POSES and PROPS from tools/gen_sprites.py and emits:
  assets/banner.svg         - Monochrome Radica Cube World banner (5 linked LCD cubes)
  assets/screenshot.svg     - 240x240 Waveshare ESP32-S3-LCD-1.54 simulation
  assets/sheets/stickman.svg - Authentic B&W stick-figure pose sheet

Usage: python3 tools/gen_images.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

from gen_sprites import POSES, PROPS  # noqa: E402

STN_OFF = "#BCC8AE"
STN_ON = "#181F16"
STN_GRID = "#B0BC9E"
BEZEL_OUT = "#181A1C"
BEZEL_IN = "#0E1012"


def render_lcd32_svg_group(rows, ox, oy, scale, prop=None, prop_xy=None, invert=False):
    w = len(rows[0])
    h = len(rows)
    grid = [[1 if invert else 0] * 32 for _ in range(32)]
    sx = (32 - w) // 2
    sy = 30 - h
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch == "#" and 0 <= sx + x < 32 and 0 <= sy + y < 32:
                grid[sy + y][sx + x] = 0 if invert else 1
    if prop and prop_xy:
        px0, py0 = prop_xy
        for y, r in enumerate(prop):
            for x, ch in enumerate(r):
                if ch == "#" and 0 <= px0 + x < 32 and 0 <= py0 + y < 32:
                    grid[py0 + y][px0 + x] = 0 if invert else 1

    cell = scale - 1 if scale >= 4 else scale
    parts = [f'<rect x="{ox}" y="{oy}" width="{32*scale}" height="{32*scale}" fill="{STN_GRID}"/>']
    for y in range(32):
        for x in range(32):
            fill = STN_ON if grid[y][x] else STN_OFF
            parts.append(
                f'<rect x="{ox + x*scale}" y="{oy + y*scale}" width="{cell}" height="{cell}" fill="{fill}"/>'
            )
    return "\n  ".join(parts)


def make_screenshot():
    # 240x240 authentic Waveshare ESP32-S3-LCD-1.54 single-cube view (7x scale = 224x224 + 8px bezel)
    svg = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="240" height="240" viewBox="0 0 240 240">',
        f'  <rect width="240" height="240" rx="10" fill="{BEZEL_OUT}"/>',
        f'  <rect x="4" y="4" width="232" height="232" rx="6" fill="{BEZEL_IN}"/>',
        "  " + render_lcd32_svg_group(POSES["WHIP1"], 8, 8, 7, PROPS["WHIP_ARC2"], (2, 10)),
        "</svg>",
    ]
    return "\n".join(svg) + "\n"


def make_banner():
    cubes = [
        ("WHIP", "0x06", "WHIP1", "WHIP_ARC1", (1, 10)),
        ("DODGER", "0x0E", "KICK1", "BALL", (2, 24)),
        ("HANS", "0x0D", "STRETCH1", "DUMBBELL", (10, 2)),
        ("HANDY", "0x0B", "MAD1", "WRENCH", (2, 11)),
        ("SCOOP", "0x01", "WAVE0", "DOG0", (19, 23)),
    ]
    scale = 4
    lcd = 32 * scale  # 128
    pad = 24
    w = len(cubes) * (lcd + 16) + (len(cubes) + 1) * pad
    h = lcd + 130
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">',
        '  <rect width="100%" height="100%" fill="#111413"/>',
        '  <text x="24" y="34" font-family="monospace" font-size="22" font-weight="bold" fill="#DCE6D0">OPEN CUBE WORLD</text>',
        '  <text x="24" y="56" font-family="monospace" font-size="13" fill="#8B9980">Authentic 32x32 1-bit monochrome Radica Cube World on Waveshare ESP32-S3-LCD-1.54</text>',
    ]
    for i, (name, pid, pose, prop, pxy) in enumerate(cubes):
        bx = pad + i * (lcd + 16 + pad)
        by = 74
        parts.append(f'  <rect x="{bx}" y="{by}" width="{lcd+16}" height="{lcd+16}" rx="8" fill="#232826" stroke="#3B423D" stroke-width="2"/>')
        parts.append("  " + render_lcd32_svg_group(POSES[pose], bx + 8, by + 8, scale, PROPS[prop], pxy))
        parts.append(
            f'  <text x="{bx + (lcd+16)//2}" y="{by + lcd + 36}" font-family="monospace" font-size="13" fill="#BCC8AE" text-anchor="middle">{name} ({pid})</text>'
        )
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def make_pose_sheet():
    show = ["IDLE0", "IDLE1", "STRETCH1", "SIT1", "MAD1", "ITCH1", "WAVE0", "WALK0", "WHIP1", "DODGE0", "DIZZY1", "TUMBLE_FLAT"]
    cols = 6
    rows_n = (len(show) + cols - 1) // cols
    scale = 3
    lcd = 32 * scale
    pad = 14
    w = cols * (lcd + pad) + pad
    h = rows_n * (lcd + 24 + pad) + 42
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">',
        '  <rect width="100%" height="100%" fill="#111413"/>',
        '  <text x="14" y="26" font-family="monospace" font-size="16" font-weight="bold" fill="#DCE6D0">Authentic 32x32 Monochrome Stickman Poses</text>',
    ]
    for idx, name in enumerate(show):
        c = idx % cols
        r = idx // cols
        x = pad + c * (lcd + pad)
        y = 40 + r * (lcd + 24 + pad)
        parts.append("  " + render_lcd32_svg_group(POSES[name], x, y, scale))
        parts.append(
            f'  <text x="{x + lcd//2}" y="{y + lcd + 16}" font-family="monospace" font-size="11" fill="#BCC8AE" text-anchor="middle">{name.lower()}</text>'
        )
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def main():
    os.makedirs(os.path.join(ROOT, "assets", "sheets"), exist_ok=True)
    with open(os.path.join(ROOT, "assets", "screenshot.svg"), "w") as f:
        f.write(make_screenshot())
    with open(os.path.join(ROOT, "assets", "banner.svg"), "w") as f:
        f.write(make_banner())
    with open(os.path.join(ROOT, "assets", "sheets", "stickman.svg"), "w") as f:
        f.write(make_pose_sheet())
    print("Generated monochrome SVG assets in assets/")


if __name__ == "__main__":
    main()
