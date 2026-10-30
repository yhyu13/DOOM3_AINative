#!/usr/bin/env python3
"""Deterministic source generator for the large-environment hero cell (spec 009, T-025).

A large industrial wall surface: 512-px panels with recessed seams and rivets, concrete grime,
stains and rust streaks. baseColor is authored as four 2048x2048 tiles that the builder assembles
into the 4096x4096 residency test surface. No assembled 4096 image is committed; the assembly is
declared in the sidecar and produced by content/tools/build-material.ps1 into git-ignored out/.

Usage:
    py -3 content/tools/generators/environment.py [--tile 2048] [--out <dir>]
"""
import argparse
import os

import hero_common as hc

VERSION = "1.0.0"
WORLD = 4096
PANEL = 512
SEAM = 8
RIVET_STEP = 128


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tile", type=int, default=2048)
    parser.add_argument("--out", default=os.path.normpath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "hero", "environment")))
    args = parser.parse_args()
    tile = args.tile
    if tile != 2048 or WORLD % tile != 0:
        raise SystemExit("this generator authors four 2048x2048 tiles for a 4096x4096 canvas")

    os.makedirs(args.out, exist_ok=True)
    field_size = 256
    stain_field = hc.noise_field(field_size, field_size, 4, 4, seed=131, octaves=3)
    grime_field = hc.noise_field(field_size, field_size, 16, 16, seed=137, octaves=2)

    for ty in range(WORLD // tile):
        for tx in range(WORLD // tile):
            x0 = tx * tile
            y0 = ty * tile
            stain = hc.sample_rect(stain_field, field_size, field_size, x0, y0, tile, tile, WORLD, WORLD)
            grime = hc.sample_rect(grime_field, field_size, field_size, x0, y0, tile, tile, WORLD, WORLD)
            base = bytearray(tile * tile * 3)
            for y in range(tile):
                wy = y0 + y
                py = wy // PANEL
                local_y = wy % PANEL
                seam_y = local_y < SEAM or local_y >= PANEL - SEAM
                along_y = (wy % RIVET_STEP) < 5 or (wy % RIVET_STEP) > RIVET_STEP - 6
                row = y * tile
                for x in range(tile):
                    wx = x0 + x
                    px = wx // PANEL
                    local_x = wx % PANEL
                    seam_x = local_x < SEAM or local_x >= PANEL - SEAM
                    along_x = (wx % RIVET_STEP) < 5 or (wx % RIVET_STEP) > RIVET_STEP - 6
                    seam = seam_x or seam_y
                    rivet = (seam_x and along_y) or (seam_y and along_x)
                    index = row + x
                    s = stain[index]
                    g = grime[index]
                    grain = hc.hash2(wx, wy, 149)
                    tint = hc.hash2(px, py, 211)

                    shade = 0.62 + 0.28 * s + 0.08 * (g - 0.5) + 0.06 * (tint - 0.5) + 0.035 * (grain - 0.5)
                    red = 118.0 * shade
                    green = 121.0 * shade
                    blue = 120.0 * shade
                    rust = hc.smoothstep(0.64, 0.84, s + 0.10 * g)
                    if rust > 0.0:
                        red = hc.mix(red, 128.0, rust * 0.55)
                        green = hc.mix(green, 84.0, rust * 0.55)
                        blue = hc.mix(blue, 58.0, rust * 0.55)
                    if seam:
                        red *= 0.52
                        green *= 0.54
                        blue *= 0.56
                    if rivet:
                        red = 154.0 * (0.8 + 0.2 * tint)
                        green = 158.0 * (0.8 + 0.2 * tint)
                        blue = 160.0 * (0.8 + 0.2 * tint)
                    out = index * 3
                    base[out] = hc.clamp8(red)
                    base[out + 1] = hc.clamp8(green)
                    base[out + 2] = hc.clamp8(blue)

            hc.write_png(os.path.join(args.out, "baseColor_x%d_y%d.png" % (tx, ty)), tile, tile, base, 3)
            print("wrote baseColor_x%d_y%d.png" % (tx, ty))

    # Normal, roughness and metalness are authored at 2048 for the whole 4096 surface.
    normal_size = 2048
    stain = hc.sample_rect(stain_field, field_size, field_size, 0, 0, normal_size, normal_size, WORLD, WORLD)
    grime = hc.sample_rect(grime_field, field_size, field_size, 0, 0, normal_size, normal_size, WORLD, WORLD)
    height = [0.0] * (normal_size * normal_size)
    roughness = bytearray(normal_size * normal_size)
    metalness = bytearray(normal_size * normal_size)
    for y in range(normal_size):
        wy = y * 2
        local_y = wy % PANEL
        seam_y = local_y < SEAM or local_y >= PANEL - SEAM
        along_y = (wy % RIVET_STEP) < 5 or (wy % RIVET_STEP) > RIVET_STEP - 6
        row = y * normal_size
        for x in range(normal_size):
            wx = x * 2
            local_x = wx % PANEL
            seam_x = local_x < SEAM or local_x >= PANEL - SEAM
            along_x = (wx % RIVET_STEP) < 5 or (wx % RIVET_STEP) > RIVET_STEP - 6
            seam = seam_x or seam_y
            rivet = (seam_x and along_y) or (seam_y and along_x)
            index = row + x
            s = stain[index]
            g = grime[index]
            grain = hc.hash2(wx, wy, 151)
            if rivet:
                h = 0.82
            elif seam:
                h = 0.18
            else:
                h = 0.52 + 0.06 * (grain - 0.5) + 0.04 * (s - 0.5)
            height[index] = h
            if rivet:
                rough = 0.30
                metal = 0.86
            elif seam:
                rough = 0.40 + 0.10 * g
                metal = 0.74
            else:
                rough = 0.80 + 0.10 * (1.0 - s) + 0.04 * (grain - 0.5)
                metal = 0.02 + 0.03 * hc.smoothstep(0.7, 0.9, s)
            roughness[index] = hc.clamp8(hc.clamp(rough) * 255.0)
            metalness[index] = hc.clamp8(hc.clamp(metal) * 255.0)

    normal = hc.normal_map(height, normal_size, strength=2.0)
    hc.write_png(os.path.join(args.out, "normal.png"), normal_size, normal_size, normal, 3)
    hc.write_png(os.path.join(args.out, "roughness.png"), normal_size, normal_size, roughness, 1)
    hc.write_png(os.path.join(args.out, "metalness.png"), normal_size, normal_size, metalness, 1)
    print("wrote normal.png, roughness.png, metalness.png at %dx%d in %s (generator %s)"
          % (normal_size, normal_size, args.out, VERSION))


if __name__ == "__main__":
    main()
