#!/usr/bin/env python3
"""Shared deterministic helpers for the Q4 hero source generators (spec 009).

The six authored cells share one small, dependency-free toolkit: a PNG writer, tileable value
noise, bilinear sampling and a height-to-normal conversion. Keeping it in one file means the
provenance record can name exactly which code produced an image, and a reviewer can re-run it
byte-for-byte.

Nothing here reads an image, a pack or a retail file. Inputs are integers and a seed.

Licence: CC0-1.0, like the images the generators write.
"""
import math
import struct
import zlib

VERSION = "1.0.0"


def write_png(path, width, height, data, channels):
    """Write an 8-bit PNG from a flat bytearray in row order.

    channels: 1 = grey, 3 = RGB, 4 = RGBA. No Pillow, so the generator has no dependency.
    """
    if channels not in (1, 3, 4):
        raise ValueError("channels must be 1, 3 or 4")
    if len(data) != width * height * channels:
        raise ValueError("pixel buffer is %d bytes, expected %d" % (len(data), width * height * channels))
    color_type = {1: 0, 3: 2, 4: 6}[channels]

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    stride = width * channels
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter: none
        raw.extend(data[y * stride:(y + 1) * stride])
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as handle:
        handle.write(png)


def hash2(x, y, seed):
    """Deterministic 0..1 hash of two integers and a seed."""
    n = (x * 374761393 + y * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    n = (n ^ (n >> 13)) * 1274126177 & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFFFFFF) / 0xFFFFFFFF


def _lattice(ix, iy, cx, cy, seed):
    return hash2(ix % cx, iy % cy, seed)


def value_noise(x, y, nx, ny, cx, cy, seed):
    """Smooth, tileable value noise on an nx-by-ny field with cx-by-cy lattice cells."""
    fx = x / nx * cx
    fy = y / ny * cy
    ix = int(fx)
    iy = int(fy)
    tx = fx - ix
    ty = fy - iy
    sx = tx * tx * (3.0 - 2.0 * tx)
    sy = ty * ty * (3.0 - 2.0 * ty)
    a = _lattice(ix, iy, cx, cy, seed)
    b = _lattice(ix + 1, iy, cx, cy, seed)
    c = _lattice(ix, iy + 1, cx, cy, seed)
    d = _lattice(ix + 1, iy + 1, cx, cy, seed)
    top = a + (b - a) * sx
    bottom = c + (d - c) * sx
    return top + (bottom - top) * sy


def noise_field(nx, ny, cx, cy, seed, octaves=3):
    """A small tileable fBm field, normalised to 0..1."""
    field = [0.0] * (nx * ny)
    amplitude = 1.0
    total = 0.0
    for octave in range(octaves):
        for y in range(ny):
            row = y * nx
            for x in range(nx):
                field[row + x] += amplitude * value_noise(x, y, nx, ny, cx, cy, seed + octave * 101)
        total += amplitude
        amplitude *= 0.5
        cx *= 2
        cy *= 2
    inv = 1.0 / total
    for index in range(len(field)):
        field[index] *= inv
    return field


def sample_rect(field, nx, ny, x0, y0, width, height, world_width, world_height):
    """Bilinear, wrapping sample of a small field over a rectangle of a larger canvas."""
    out = [0.0] * (width * height)
    for y in range(height):
        fy = (y0 + y + 0.5) / world_height * ny - 0.5
        iy = math.floor(fy)
        ty = fy - iy
        iy0 = iy % ny
        iy1 = (iy + 1) % ny
        rowa = iy0 * nx
        rowb = iy1 * nx
        dst = y * width
        for x in range(width):
            fx = (x0 + x + 0.5) / world_width * nx - 0.5
            ix = math.floor(fx)
            tx = fx - ix
            ix0 = ix % nx
            ix1 = (ix + 1) % nx
            a = field[rowa + ix0]
            b = field[rowa + ix1]
            c = field[rowb + ix0]
            d = field[rowb + ix1]
            top = a + (b - a) * tx
            bottom = c + (d - c) * tx
            out[dst + x] = top + (bottom - top) * ty
    return out


def normal_map(height, size, strength, channels=3):
    """Tangent-space normal from a wrapping height field (OpenGL +Y up)."""
    out = bytearray(size * size * channels)
    for y in range(size):
        prev = ((y - 1) % size) * size
        next_row = ((y + 1) % size) * size
        row = y * size
        for x in range(size):
            left = height[row + ((x - 1) % size)]
            right = height[row + ((x + 1) % size)]
            down = height[prev + x]
            up = height[next_row + x]
            nx = -(right - left) * strength
            ny = -(up - down) * strength
            nz = 1.0
            inv = 1.0 / math.sqrt(nx * nx + ny * ny + nz * nz)
            index = (row + x) * channels
            out[index] = _clamp8((nx * inv * 0.5 + 0.5) * 255.0)
            out[index + 1] = _clamp8((ny * inv * 0.5 + 0.5) * 255.0)
            if channels == 3:
                out[index + 2] = _clamp8((nz * inv * 0.5 + 0.5) * 255.0)
    return out


def _clamp8(value):
    if value <= 0.0:
        return 0
    if value >= 255.0:
        return 255
    return int(value + 0.5)


# Public alias for generators; the underscore name is kept because normal_map predates it.
clamp8 = _clamp8


def clamp(value, low=0.0, high=1.0):
    if value < low:
        return low
    if value > high:
        return high
    return value


def smoothstep(edge0, edge1, value):
    if edge1 == edge0:
        return 0.0
    t = clamp((value - edge0) / (edge1 - edge0))
    return t * t * (3.0 - 2.0 * t)


def mix(a, b, t):
    return a + (b - a) * t


def write_grey(path, size, values, scale=255.0):
    """Write a single-channel PNG from 0..1 floats."""
    data = bytearray(size * size)
    for index, value in enumerate(values):
        data[index] = _clamp8(value * scale)
    write_png(path, size, size, data, 1)
