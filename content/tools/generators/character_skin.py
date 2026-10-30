#!/usr/bin/env python3
"""Deterministic source generator for the skinned-character hero cell (spec 009, T-023).

A worn character-suit surface: a woven twill with stitched seams, creases and scuffs. The
material is what 009 can author; the rigged mesh, skeleton and animation that exercise skinning
are outside the material contract and are recorded as a dependency, not fabricated here.

Usage:
    py -3 content/tools/generators/character_skin.py [--size 512] [--out <dir>]
"""
import argparse
import os

import hero_common as hc

VERSION = "1.0.0"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=512)
    parser.add_argument("--out", default=os.path.normpath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "hero", "character_skin")))
    args = parser.parse_args()
    size = args.size
    if size % 4 != 0 or size > 2048:
        raise SystemExit("size must be a multiple of 4 and <= 2048")

    os.makedirs(args.out, exist_ok=True)
    field_size = 128
    fold_field = hc.noise_field(field_size, field_size, 8, 8, seed=83, octaves=3)
    dirt_field = hc.noise_field(field_size, field_size, 16, 16, seed=89, octaves=2)
    folds = hc.sample_rect(fold_field, field_size, field_size, 0, 0, size, size, size, size)
    dirt = hc.sample_rect(dirt_field, field_size, field_size, 0, 0, size, size, size, size)

    base = bytearray(size * size * 3)
    roughness = bytearray(size * size)
    height = [0.0] * (size * size)

    for y in range(size):
        row = y * size
        for x in range(size):
            index = row + x
            weave = 1.0 if ((x // 3 + y // 3) % 2 == 0) else 0.0
            thread = 1.0 if (x % 3 == 0 or y % 3 == 0) else 0.0
            seam = 0.0
            if 126 <= x <= 132 or 382 <= x <= 388 or 126 <= y <= 132 or 382 <= y <= 388:
                seam = 1.0

            f = folds[index]
            d = dirt[index]
            grain = hc.hash2(x, y, 91)
            worn = hc.smoothstep(0.62, 0.86, d + 0.18 * f)

            fabric = 0.58 + 0.16 * f + 0.05 * weave - 0.03 * thread + 0.04 * (grain - 0.5)
            if seam > 0.0:
                fabric *= 0.78
            red = 88.0 * fabric
            green = 96.0 * fabric
            blue = 84.0 * fabric
            if worn > 0.0:
                red += 24.0 * worn
                green += 22.0 * worn
                blue += 18.0 * worn
            if seam > 0.0:
                red *= 0.86
                green *= 0.86
                blue *= 0.86

            out = index * 3
            base[out] = hc.clamp8(red)
            base[out + 1] = hc.clamp8(green)
            base[out + 2] = hc.clamp8(blue)

            rough = 0.76 + 0.10 * d - 0.24 * worn + 0.05 * seam
            roughness[index] = hc.clamp8(hc.clamp(rough) * 255.0)

            h = 0.22 * f + 0.05 * weave + 0.03 * thread + 0.55 * seam - 0.08 * worn + 0.02 * grain
            height[index] = h

    normal = hc.normal_map(height, size, strength=2.6)
    hc.write_png(os.path.join(args.out, "baseColor.png"), size, size, base, 3)
    hc.write_png(os.path.join(args.out, "normal.png"), size, size, normal, 3)
    hc.write_png(os.path.join(args.out, "roughness.png"), size, size, roughness, 1)
    print("wrote 3 files at %dx%d in %s (generator %s)" % (size, size, args.out, VERSION))


if __name__ == "__main__":
    main()
