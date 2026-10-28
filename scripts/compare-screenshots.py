#!/usr/bin/env python3
"""Compare two same-size screenshots and report a conservative pixel-difference metric.

Exit 0 when at least --threshold percent of pixels differ by --delta or more in any RGB
channel, else exit 1. The metric is deliberately simple so the result can be reproduced
without a perceptual metric implementation.

Usage:
    py -3 scripts/compare-screenshots.py --base baseline.png --hd hd.png [--delta 8] [--threshold 1.0]
"""
import argparse
import sys

from PIL import Image, ImageChops, ImageStat


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--hd", required=True)
    ap.add_argument("--delta", type=int, default=8, help="per-channel difference that counts (0-255)")
    ap.add_argument("--threshold", type=float, default=1.0, help="percent of pixels that must differ")
    ap.add_argument("--out", help="optional diff image path")
    args = ap.parse_args()

    a = Image.open(args.base).convert("RGB")
    b = Image.open(args.hd).convert("RGB")
    if a.size != b.size:
        print(f"RESULT=FAIL SIZE_MISMATCH base={a.size} hd={b.size}")
        return 1

    # Max absolute difference across the three channels, per pixel.
    diff = ImageChops.difference(a, b)
    r, g, bl = diff.split()
    maxdiff = ImageChops.lighter(ImageChops.lighter(r, g), bl)
    if args.out:
        maxdiff.save(args.out)

    hist = maxdiff.histogram()
    total = a.size[0] * a.size[1]
    changed = sum(hist[args.delta:])
    percent = 100.0 * changed / total if total else 0.0
    mean_abs = ImageStat.Stat(diff).mean
    mean_abs_value = sum(mean_abs) / len(mean_abs)

    print(f"PIXELS={total}")
    print(f"DIFF_PIXELS={changed}")
    print(f"DIFF_PERCENT={percent:.4f}")
    print(f"MEAN_ABS={mean_abs_value:.4f}")
    print(f"DELTA={args.delta}")
    print(f"THRESHOLD={args.threshold}")
    ok = percent >= args.threshold
    print(f"RESULT={'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
