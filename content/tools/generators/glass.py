#!/usr/bin/env python3
"""Deterministic source generator for the transmissive-glass hero cell (spec 009, T-024).

A pale, slightly frosted glass pane: a low-roughness face with soft ripples and smudges, and a
thicker, more opaque edge. Alpha is authored in baseColor; refraction and opacity modelling are
owned by spec 004 and are recorded as a dependency instead of being invented here.

Usage:
    py -3 content/tools/generators/glass.py [--size 512] [--out <dir>]
"""
import argparse
import os

import hero_common as hc

VERSION = "1.0.0"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=512)
    parser.add_argument("--out", default=os.path.normpath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "hero", "glass")))
    args = parser.parse_args()
    size = args.size
    if size % 4 != 0 or size > 2048:
        raise SystemExit("size must be a multiple of 4 and <= 2048")

    os.makedirs(args.out, exist_ok=True)
    field_size = 128
    ripple_field = hc.noise_field(field_size, field_size, 4, 4, seed=101, octaves=3)
    smudge_field = hc.noise_field(field_size, field_size, 16, 16, seed=103, octaves=2)
    ripple = hc.sample_rect(ripple_field, field_size, field_size, 0, 0, size, size, size, size)
    smudge = hc.sample_rect(smudge_field, field_size, field_size, 0, 0, size, size, size, size)

    base = bytearray(size * size * 4)
    roughness = bytearray(size * size)
    height = [0.0] * (size * size)

    for y in range(size):
        row = y * size
        for x in range(size):
            index = row + x
            edge = min(x, y, size - 1 - x, size - 1 - y)
            bevel = hc.smoothstep(0.0, 26.0, edge)
            r = ripple[index]
            s = smudge[index]
            grain = hc.hash2(x, y, 107)

            tint = 0.72 + 0.16 * r + 0.04 * (grain - 0.5)
            red = hc.mix(132.0, 176.0, tint)
            green = hc.mix(176.0, 208.0, tint)
            blue = hc.mix(184.0, 214.0, tint)
            alpha = hc.mix(242.0, 196.0, bevel) + 14.0 * (s - 0.5)

            out = index * 4
            base[out] = hc.clamp8(red)
            base[out + 1] = hc.clamp8(green)
            base[out + 2] = hc.clamp8(blue)
            base[out + 3] = hc.clamp8(hc.clamp(alpha / 255.0) * 255.0)

            rough = hc.mix(0.30, 0.055 + 0.10 * s + 0.04 * r, bevel)
            roughness[index] = hc.clamp8(hc.clamp(rough) * 255.0)
            height[index] = 0.55 * (1.0 - bevel) + 0.18 * r + 0.03 * grain

    normal = hc.normal_map(height, size, strength=0.9)
    hc.write_png(os.path.join(args.out, "baseColor.png"), size, size, base, 4)
    hc.write_png(os.path.join(args.out, "normal.png"), size, size, normal, 3)
    hc.write_png(os.path.join(args.out, "roughness.png"), size, size, roughness, 1)
    print("wrote 3 files at %dx%d in %s (generator %s)" % (size, size, args.out, VERSION))


if __name__ == "__main__":
    main()
