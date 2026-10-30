#!/usr/bin/env python3
"""Deterministic source generator for the emissive hero cell (spec 009, T-022).

A painted sign panel with an abstract illuminated glyph block. The emissive source is an
authored 8-bit luminance mask: the bars keep their internal gradient and the glow keeps its
falloff, so the image is not a clipped white blob. The HDR half of the cell is recorded as
blocked in the sidecar, because the pinned BC7_UNORM_SRGB path cannot carry radiance above 1.0.

Usage:
    py -3 content/tools/generators/emissive_sign.py [--size 512] [--out <dir>]
"""
import argparse
import os

import hero_common as hc

VERSION = "1.0.0"

# Absolute block layout on a 512x512 canvas. Each entry is (x0, y0, x1, y1).
GLYPHS = [
    (96, 150, 168, 178), (196, 150, 236, 178), (264, 150, 336, 178), (364, 150, 420, 178),
    (96, 206, 140, 234), (168, 206, 208, 234), (236, 206, 300, 234), (328, 206, 420, 234),
    (96, 262, 180, 290), (208, 262, 248, 290), (276, 262, 340, 290), (368, 262, 420, 290),
    (96, 318, 136, 346), (164, 318, 228, 346), (256, 318, 316, 346), (344, 318, 420, 346),
]


def _blur(values, size, radius):
    temp = [0.0] * (size * size)
    for y in range(size):
        row = y * size
        for x in range(size):
            total = 0.0
            count = 0
            for dx in range(-radius, radius + 1):
                nx = x + dx
                if 0 <= nx < size:
                    total += values[row + nx]
                    count += 1
            temp[row + x] = total / count
    out = [0.0] * (size * size)
    for y in range(size):
        for x in range(size):
            total = 0.0
            count = 0
            for dy in range(-radius, radius + 1):
                ny = y + dy
                if 0 <= ny < size:
                    total += temp[ny * size + x]
                    count += 1
            out[y * size + x] = total / count
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=512)
    parser.add_argument("--out", default=os.path.normpath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "hero", "emissive_sign")))
    args = parser.parse_args()
    size = args.size
    if size != 512:
        raise SystemExit("this generator's layout is authored for 512x512")

    os.makedirs(args.out, exist_ok=True)
    field_size = 128
    paint_field = hc.noise_field(field_size, field_size, 8, 8, seed=71, octaves=3)
    paint = hc.sample_rect(paint_field, field_size, field_size, 0, 0, size, size, size, size)

    panel_x0, panel_y0, panel_x1, panel_y1 = 48, 80, 464, 432
    frame = 10
    mask = [0.0] * (size * size)
    for (x0, y0, x1, y1) in GLYPHS:
        for y in range(y0, y1):
            row = y * size
            for x in range(x0, x1):
                mask[row + x] = 1.0
    glow = _blur(mask, size, 5)

    base = bytearray(size * size * 3)
    emissive = bytearray(size * size)
    roughness = bytearray(size * size)
    metalness = bytearray(size * size)
    height = [0.0] * (size * size)

    for y in range(size):
        row = y * size
        for x in range(size):
            index = row + x
            grain = hc.hash2(x, y, 79)
            inside = panel_x0 <= x < panel_x1 and panel_y0 <= y < panel_y1
            on_frame = inside and (
                x < panel_x0 + frame or x >= panel_x1 - frame
                or y < panel_y0 + frame or y >= panel_y1 - frame)
            glyph = mask[index] > 0.5

            if inside:
                p = paint[index]
                if on_frame:
                    shade = 0.44 + 0.18 * p + 0.05 * (grain - 0.5)
                    red, green, blue = 126.0 * shade, 132.0 * shade, 140.0 * shade
                    rough = 0.26 + 0.12 * p
                    metal = 0.72 - 0.10 * p
                    h = 0.66 + 0.04 * p
                else:
                    shade = 0.30 + 0.14 * p + 0.04 * (grain - 0.5)
                    red, green, blue = 64.0 * shade, 70.0 * shade, 82.0 * shade
                    if glyph:
                        red, green, blue = 26.0, 30.0, 34.0
                    rough = 0.44 + 0.12 * p
                    metal = 0.02
                    h = 0.18 + 0.02 * p
            else:
                shade = 0.22 + 0.08 * (grain - 0.5)
                red, green, blue = 28.0 * shade, 31.0 * shade, 36.0 * shade
                rough = 0.55
                metal = 0.0
                h = 0.0

            out = index * 3
            base[out] = hc.clamp8(red)
            base[out + 1] = hc.clamp8(green)
            base[out + 2] = hc.clamp8(blue)
            roughness[index] = hc.clamp8(hc.clamp(rough) * 255.0)
            metalness[index] = hc.clamp8(hc.clamp(metal) * 255.0)
            height[index] = h

            if inside and not on_frame:
                core = 1.0 if glyph else 0.0
                value = core if core > 0.0 else glow[index] * 0.90
                # A faint panel wash keeps the unlit paint from reading as pure black.
                value = max(value, 0.035)
                emissive[index] = hc.clamp8(hc.clamp(value) * 255.0)
            else:
                emissive[index] = 0

    normal = hc.normal_map(height, size, strength=1.8)
    hc.write_png(os.path.join(args.out, "baseColor.png"), size, size, base, 3)
    hc.write_png(os.path.join(args.out, "normal.png"), size, size, normal, 3)
    hc.write_png(os.path.join(args.out, "roughness.png"), size, size, roughness, 1)
    hc.write_png(os.path.join(args.out, "metalness.png"), size, size, metalness, 1)
    hc.write_png(os.path.join(args.out, "emissive.png"), size, size, emissive, 1)
    print("wrote 5 files at %dx%d in %s (generator %s)" % (size, size, args.out, VERSION))


if __name__ == "__main__":
    main()
