#!/usr/bin/env python3
"""Deterministic source generator for the dielectric hero material (spec 009, US1).

Why a generator instead of three opaque PNGs: a reviewer can read this file, run it, and get byte-
identical output, so the committed images are reproducible rather than unexplained. It is CC0-1.0 like
the images it writes.

The material is a painted, slightly worn dielectric panel: no metalness, subtle orange-peel normal,
and roughness variation where the paint is scuffed. Nothing here is derived from any game asset.

Usage:
    py -3 content/tools/generators/dielectric.py [--size 512] [--out <dir>]
"""
import argparse
import math
import os
import struct
import zlib


def _write_png(path, width, height, pixels, channels):
    """Write an 8-bit PNG without Pillow, so the generator has no dependency at all."""
    color_type = 2 if channels == 3 else 6

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter: none
        row = pixels[y * width:(y + 1) * width]
        for px in row:
            raw.extend(px)
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as fh:
        fh.write(png)


def _hash2(x, y, seed):
    n = (x * 374761393 + y * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    n = (n ^ (n >> 13)) * 1274126177 & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFFFFFF) / 0xFFFFFFFF


def _value_noise(x, y, period, seed):
    """Smooth value noise on a wrapping lattice, so the texture tiles without a seam."""
    def lattice(ix, iy):
        return _hash2(ix % period, iy % period, seed)

    fx = (x % period) / period * period
    fy = (y % period) / period * period
    ix, iy = int(fx), int(fy)
    tx, ty = fx - ix, fy - iy
    sx = tx * tx * (3 - 2 * tx)
    sy = ty * ty * (3 - 2 * ty)
    a = lattice(ix, iy)
    b = lattice(ix + 1, iy)
    c = lattice(ix, iy + 1)
    d = lattice(ix + 1, iy + 1)
    return (a * (1 - sx) + b * sx) * (1 - sy) + (c * (1 - sx) + d * sx) * sy


def _height(x, y, size):
    """Two octaves of tileable noise plus a shallow orange-peel ripple."""
    base = _value_noise(x, y, 16, 11) * 0.65 + _value_noise(x, y, 64, 29) * 0.35
    peel = 0.5 + 0.5 * math.sin(2 * math.pi * x / size * 12) * math.sin(2 * math.pi * y / size * 12)
    return base * 0.85 + peel * 0.15


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=512)
    ap.add_argument("--out", default=os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "hero", "dielectric")))
    args = ap.parse_args()
    size = args.size
    if size % 4 != 0 or size > 2048:
        raise SystemExit("size must be a multiple of 4 and <= 2048")

    os.makedirs(args.out, exist_ok=True)
    albedo, normal, roughness = [], [], []
    height = [[_height(x, y, size) for x in range(size)] for y in range(size)]

    for y in range(size):
        for x in range(size):
            h = height[y][x]
            wear = _value_noise(x, y, 8, 71)
            # Albedo: warm grey paint, darker in the recesses, with faint speckle.
            speck = (_hash2(x, y, 97) - 0.5) * 0.05
            shade = 0.62 + 0.18 * h - 0.06 * wear + speck
            albedo.append((
                max(0, min(255, int(shade * 214))),
                max(0, min(255, int(shade * 206))),
                max(0, min(255, int(shade * 196))),
                255,
            ))
            # Tangent-space normal from central differences on the height field (OpenGL +Y up).
            hx = height[y][(x + 1) % size] - height[y][(x - 1) % size]
            hy = height[(y + 1) % size][x] - height[(y - 1) % size][x]
            nx, ny, nz = -hx * 6.0, -hy * 6.0, 1.0
            inv = 1.0 / math.sqrt(nx * nx + ny * ny + nz * nz)
            normal.append((
                max(0, min(255, int((nx * inv * 0.5 + 0.5) * 255))),
                max(0, min(255, int((ny * inv * 0.5 + 0.5) * 255))),
                max(0, min(255, int((nz * inv * 0.5 + 0.5) * 255))),
                255,
            ))
            # Roughness: smoother where the paint is intact, rougher where it is scuffed.
            r = 0.42 + 0.34 * wear + 0.08 * (1.0 - h)
            r = max(0.0, min(1.0, r))
            v = int(r * 255)
            roughness.append((v, v, v, 255))

    _write_png(os.path.join(args.out, "baseColor.png"), size, size, albedo, 4)
    _write_png(os.path.join(args.out, "normal.png"), size, size, normal, 4)
    _write_png(os.path.join(args.out, "roughness.png"), size, size, roughness, 4)
    print("wrote baseColor.png, normal.png, roughness.png at %dx%d in %s" % (size, size, args.out))


if __name__ == "__main__":
    main()
