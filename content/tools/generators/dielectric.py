#!/usr/bin/env python3
"""Deterministic source generator for the dielectric hero material (spec 009, US1).

Why a generator instead of three opaque PNGs: a reviewer can read this file, run it, and get byte-
identical output, so the committed images are reproducible rather than unexplained. It is CC0-1.0 like
the images it writes.

The material is a painted, slightly worn dielectric panel: no metalness, subtle orange-peel normal,
and roughness variation where the paint is scuffed. Nothing here is derived from any game asset.

Everything spatial comes from `hero_common`, the same tileable-value-noise toolkit the other six hero
generators use. This file used to carry a private copy of that helper, and the copy was wrong: its
lattice coordinate was `(x % period) / period * period`, which is just `x % period`, so the
interpolation weight was always zero and every "smooth" field degenerated into a per-texel hash. The
normal map built from that field had a mean tilt of 58 degrees and a mean neighbour-to-neighbour
angular delta of 73 degrees, i.e. white noise, and it rendered as a field of isolated specular
spikes instead of a painted panel (see specs/004-quality-renderer/review-q4.md, blocker B2). Using
the shared helper removes the duplicate that caused it; there is now one implementation to test.

Usage:
    py -3 content/tools/generators/dielectric.py [--size 512] [--out <dir>]
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import hero_common as hc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=512)
    ap.add_argument("--out", default=os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "hero", "dielectric")))
    args = ap.parse_args()
    size = args.size
    if size % 4 != 0 or size > 2048:
        raise SystemExit("size must be a multiple of 4 and <= 2048")

    os.makedirs(args.out, exist_ok=True)

    # Broad paint undulation, a finer orange-peel grain, and a very broad scuff field. Every octave
    # is a tileable value-noise field, so no texel is independent of its neighbours.
    undulation = hc.noise_field(size, size, 4, 4, seed=11, octaves=3)
    peel = hc.noise_field(size, size, 48, 48, seed=29, octaves=2)
    scuff = hc.noise_field(size, size, 6, 6, seed=71, octaves=2)

    height = [0.0] * (size * size)
    mottle = [0.0] * (size * size)
    for index in range(size * size):
        # The peel is noise, not a sinusoid: an integer-frequency sin() tiles
        # correctly but leaves a visible diagonal weave on a large plane.
        height[index] = 0.55 * undulation[index] + 0.45 * peel[index]
        mottle[index] = 0.65 * undulation[index] + 0.35 * peel[index]

    albedo = bytearray(size * size * 4)
    roughness = bytearray(size * size * 4)
    for index in range(size * size):
        wear = scuff[index]
        # Albedo: warm grey paint, slightly darker where it is scuffed. All variation is
        # spatial noise at the authored frequencies; there is no per-texel hash term.
        shade = 0.66 + 0.10 * mottle[index] - 0.06 * wear
        base = index * 4
        albedo[base] = hc.clamp8(shade * 214)
        albedo[base + 1] = hc.clamp8(shade * 206)
        albedo[base + 2] = hc.clamp8(shade * 196)
        albedo[base + 3] = 255
        # Roughness: smoother where the paint is intact, rougher where it is scuffed.
        r = 0.44 + 0.28 * wear + 0.05 * (1.0 - mottle[index])
        value = hc.clamp8(max(0.0, min(1.0, r)) * 255)
        roughness[base] = value
        roughness[base + 1] = value
        roughness[base + 2] = value
        roughness[base + 3] = 255

    # normals are a three-channel image (hero_common only writes Z when channels == 3)
    normal = hc.normal_map(height, size, strength=2.0, channels=3)

    hc.write_png(os.path.join(args.out, "baseColor.png"), size, size, albedo, 4)
    hc.write_png(os.path.join(args.out, "normal.png"), size, size, normal, 3)
    hc.write_png(os.path.join(args.out, "roughness.png"), size, size, roughness, 4)
    print("wrote baseColor.png, normal.png, roughness.png at %dx%d in %s" % (size, size, args.out))


if __name__ == "__main__":
    main()
