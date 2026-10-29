#!/usr/bin/env python3
"""Generate the committed RXGB conversion fixture (spec 009, FR-008).

Two outputs, both 16x16 and both deterministic:

  golden-rxgb.png    the legacy arrangement: x in alpha, y in red, z dropped, blue unused
  golden-linear.png  the expected result: 8-bit RGB with z reconstructed in blue

The truth behind the fixture is a fixed normal field defined in the script, not a rendered or captured
image, so the test proves the arithmetic of the converter and nothing about any retail asset. That is
deliberate: C-04 forbids a corpus capture in a PR, and this fixture needs none.

Only Pillow is required. Run from the repository root:

    py -3 tests/content/fixtures/make-golden.py
"""
import math
import os

from PIL import Image

SIZE = 16


def truth_normal(x, y):
    """A smooth, non-planar normal field, so an axis swap or a dropped Z cannot pass by accident."""
    fx = x / (SIZE - 1)
    fy = y / (SIZE - 1)
    nx = (fx - 0.5) * 1.4
    ny = (fy - 0.5) * 1.4
    nz = math.sqrt(max(0.0, 1.0 - nx * nx - ny * ny))
    return nx, ny, nz


def encode(v):
    return max(0, min(255, int(round((v * 0.5 + 0.5) * 255.0))))


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    rxgb = Image.new("RGBA", (SIZE, SIZE))
    linear = Image.new("RGB", (SIZE, SIZE))

    for y in range(SIZE):
        for x in range(SIZE):
            nx, ny, nz = truth_normal(x, y)
            ex, ey = encode(nx), encode(ny)
            # RXGB as declared by content/tools/convert-normal.ps1: x in alpha, y in red.
            rxgb.putpixel((x, y), (ey, 0, 0, ex))
            # The expected output is decoded from the stored bytes, not from the unquantised truth: a
            # correct converter can only see the bytes, so predicting z from the float value would test
            # quantisation error instead of the conversion. An axis swap or a dropped Z still changes the
            # result by far more than the tolerance.
            dx = ex / 255.0 * 2.0 - 1.0
            dy = ey / 255.0 * 2.0 - 1.0
            dz = math.sqrt(max(0.0, 1.0 - dx * dx - dy * dy))
            linear.putpixel((x, y), (encode(dx), encode(dy), encode(dz)))

    rxgb.save(os.path.join(here, "golden-rxgb.png"))
    linear.save(os.path.join(here, "golden-linear.png"))
    print("wrote golden-rxgb.png and golden-linear.png (%dx%d)" % (SIZE, SIZE))


if __name__ == "__main__":
    main()
