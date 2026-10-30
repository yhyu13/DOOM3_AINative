#!/usr/bin/env python3
"""Deterministic source generator for the opaque-metal hero cell (spec 009, T-020).

The material is a brushed, lightly oxidised steel panel: directional streaks along X, a broad
wear field, and a handful of shallow scratches. Every value is a function of (x, y, seed); no
image, pack or retail file is read.

Usage:
    py -3 content/tools/generators/metal.py [--size 512] [--out <dir>]
"""
import argparse
import os

import hero_common as hc

VERSION = "1.0.0"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=512)
    parser.add_argument("--out", default=os.path.normpath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "hero", "metal")))
    args = parser.parse_args()
    size = args.size
    if size % 4 != 0 or size > 2048:
        raise SystemExit("size must be a multiple of 4 and <= 2048")

    os.makedirs(args.out, exist_ok=True)
    field_size = 128
    # Long in X, short in Y: the brushing direction.
    streak_field = hc.noise_field(field_size, field_size, 4, 32, seed=11, octaves=2)
    patch_field = hc.noise_field(field_size, field_size, 8, 8, seed=29, octaves=3)
    streak = hc.sample_rect(streak_field, field_size, field_size, 0, 0, size, size, size, size)
    patch = hc.sample_rect(patch_field, field_size, field_size, 0, 0, size, size, size, size)

    base = bytearray(size * size * 3)
    roughness = bytearray(size * size)
    metalness = bytearray(size * size)
    height = [0.0] * (size * size)

    for y in range(size):
        row = y * size
        for x in range(size):
            index = row + x
            s = streak[index]
            p = patch[index]
            grain = hc.hash2(x, y, 97)

            # A broken horizontal scratch, repeatable because the period divides the canvas.
            offset = int(hc.hash2(x // 32, 0, 151) * 8.0)
            scratch = 1.0 if ((y + offset) % 64 == 0 and hc.hash2(x // 4, y, 157) > 0.55) else 0.0

            shade = 0.55 + 0.30 * s + 0.10 * (p - 0.5) + 0.05 * (grain - 0.5)
            oxide = hc.smoothstep(0.30, 0.10, p)
            red = hc.mix(172.0 * shade, 126.0, oxide * 0.62)
            green = hc.mix(176.0 * shade, 94.0, oxide * 0.62)
            blue = hc.mix(182.0 * shade, 70.0, oxide * 0.62)
            if scratch > 0.0:
                red += 18.0
                green += 18.0
                blue += 18.0

            out = index * 3
            base[out] = hc.clamp8(red)
            base[out + 1] = hc.clamp8(green)
            base[out + 2] = hc.clamp8(blue)

            rough = 0.16 + 0.22 * (1.0 - s) + 0.25 * p + 0.20 * oxide - 0.06 * scratch
            roughness[index] = hc.clamp8(hc.clamp(rough) * 255.0)
            metal = 0.93 - 0.38 * oxide - 0.08 * (1.0 - s) - 0.10 * scratch
            metalness[index] = hc.clamp8(hc.clamp(metal) * 255.0)

            height[index] = 0.55 * s + 0.35 * p + 0.10 * grain - 0.18 * scratch

    normal = hc.normal_map(height, size, strength=2.4)
    hc.write_png(os.path.join(args.out, "baseColor.png"), size, size, base, 3)
    hc.write_png(os.path.join(args.out, "normal.png"), size, size, normal, 3)
    hc.write_png(os.path.join(args.out, "roughness.png"), size, size, roughness, 1)
    hc.write_png(os.path.join(args.out, "metalness.png"), size, size, metalness, 1)
    print("wrote 4 files at %dx%d in %s (generator %s)" % (size, size, args.out, VERSION))


if __name__ == "__main__":
    main()
