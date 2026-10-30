#!/usr/bin/env python3
"""Deterministic source generator for the alpha-tested hero cell (spec 009, T-021).

A perforated galvanised-steel grating: round holes punched through a plate, with a solid border
frame and rust around the cut edges. The alpha channel is the cutout; the material file records
that the engine-side alpha test is owned by spec 004/008 and is not changed here.

Usage:
    py -3 content/tools/generators/grating.py [--size 512] [--out <dir>]
"""
import argparse
import math
import os

import hero_common as hc

VERSION = "1.0.0"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=512)
    parser.add_argument("--out", default=os.path.normpath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "hero", "grating")))
    args = parser.parse_args()
    size = args.size
    if size % 4 != 0 or size > 2048:
        raise SystemExit("size must be a multiple of 4 and <= 2048")

    os.makedirs(args.out, exist_ok=True)
    cell = 64
    hole_radius = 22.0
    field_size = 128
    rust_field = hc.noise_field(field_size, field_size, 8, 8, seed=41, octaves=3)
    grime_field = hc.noise_field(field_size, field_size, 16, 16, seed=43, octaves=2)
    rust_source = hc.sample_rect(rust_field, field_size, field_size, 0, 0, size, size, size, size)
    grime = hc.sample_rect(grime_field, field_size, field_size, 0, 0, size, size, size, size)

    base = bytearray(size * size * 4)
    roughness = bytearray(size * size)
    metalness = bytearray(size * size)
    height = [0.0] * (size * size)

    for y in range(size):
        row = y * size
        for x in range(size):
            index = row + x
            cx = (x % cell) - cell * 0.5
            cy = (y % cell) - cell * 0.5
            distance = math.sqrt(cx * cx + cy * cy)
            border = x < 12 or x >= size - 12 or y < 12 or y >= size - 12
            hole = (distance < hole_radius) and not border
            edge = hc.smoothstep(hole_radius, hole_radius + 5.0, distance)
            p = rust_source[index]
            g = grime[index]
            grain = hc.hash2(x, y, 61)

            if hole:
                alpha = 0
                red = green = blue = 18
                h = 0.0
            else:
                alpha = 255
                rust = hc.smoothstep(0.58, 0.78, p + 0.10 * (1.0 - edge) - 0.05 * g)
                shade = 0.52 + 0.24 * g + 0.08 * (grain - 0.5) + 0.10 * edge
                red = hc.mix(146.0 * shade, 132.0, rust * 0.85)
                green = hc.mix(152.0 * shade, 78.0, rust * 0.85)
                blue = hc.mix(154.0 * shade, 44.0, rust * 0.85)
                if border:
                    red *= 0.85
                    green *= 0.85
                    blue *= 0.85
                h = (0.18 + 0.62 * edge) if border else (0.10 + 0.70 * edge)
                h += 0.04 * (grain - 0.5)

                rough = 0.30 + 0.34 * rust + 0.12 * (1.0 - edge) + 0.06 * g
                roughness[index] = hc.clamp8(hc.clamp(rough) * 255.0)
                metal = 0.92 - 0.68 * rust - 0.10 * (1.0 - edge)
                metalness[index] = hc.clamp8(hc.clamp(metal) * 255.0)

            out = index * 4
            base[out] = hc.clamp8(red)
            base[out + 1] = hc.clamp8(green)
            base[out + 2] = hc.clamp8(blue)
            base[out + 3] = alpha
            height[index] = h

    normal = hc.normal_map(height, size, strength=3.2)
    hc.write_png(os.path.join(args.out, "baseColor.png"), size, size, base, 4)
    hc.write_png(os.path.join(args.out, "normal.png"), size, size, normal, 3)
    hc.write_png(os.path.join(args.out, "roughness.png"), size, size, roughness, 1)
    hc.write_png(os.path.join(args.out, "metalness.png"), size, size, metalness, 1)
    print("wrote 4 files at %dx%d in %s (generator %s)" % (size, size, args.out, VERSION))


if __name__ == "__main__":
    main()
