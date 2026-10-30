#!/usr/bin/env python3
"""Frame-to-frame and adapter-to-adapter differences for the T056 numbers.

The T057 review (finding N3/N4) could not reproduce the recorded motion and WARP
counts because the metric was not stated. This script states it and prints the
number, so the values in ../measurements.json are recomputable from the
committed PNGs and from the pre-fix frames in git history.

Metric, per pixel: `d = max over R,G,B of |a - b|` on the 8-bit sRGB values.
  mean_abs   = mean of d over all pixels
  max_abs    = max of d
  changed    = pixels with d > 2
  bbox       = bounding box of those pixels, [x0, y0, x1, y1], x1/y1 inclusive

Usage:
  py -3 frame_diff.py a.png b.png
  py -3 frame_diff.py --series f1.png f2.png ... f8.png    (adjacent pairs + first vs last)
"""
import json
import sys

import defect_metrics as dm


def load_rgb(path):
    w, h, channels, rows = dm.read_png(path)
    out = []
    for y in range(h):
        row = rows[y]
        out.append([(row[x * channels], row[x * channels + 1], row[x * channels + 2]) for x in range(w)])
    return w, h, out


def diff(a, b):
    wa, ha, pa = load_rgb(a)
    wb, hb, pb = load_rgb(b)
    if (wa, ha) != (wb, hb):
        raise SystemExit("extent mismatch: %s %dx%d vs %s %dx%d" % (a, wa, ha, b, wb, hb))
    total = 0
    count = 0
    maxd = 0
    changed = 0
    x0 = y0 = 10 ** 9
    x1 = y1 = -1
    for y in range(ha):
        ra, rb = pa[y], pb[y]
        for x in range(wa):
            pa_px, pb_px = ra[x], rb[x]
            d = max(abs(pa_px[0] - pb_px[0]), abs(pa_px[1] - pb_px[1]), abs(pa_px[2] - pb_px[2]))
            total += d
            count += 1
            if d > maxd:
                maxd = d
            if d > 2:
                changed += 1
                if x < x0:
                    x0 = x
                if x > x1:
                    x1 = x
                if y < y0:
                    y0 = y
                if y > y1:
                    y1 = y
    return {
        "a": a,
        "b": b,
        "mean_abs": round(total / float(count), 4),
        "max_abs": maxd,
        "changed_pixels_gt2": changed,
        "changed_pct_gt2": round(100.0 * changed / count, 3),
        "changed_bbox_xyxy": ([x0, y0, x1, y1] if x1 >= 0 else None),
    }


def main():
    args = sys.argv[1:]
    series = False
    if args and args[0] == "--series":
        series = True
        args = args[1:]
    if len(args) < 2:
        raise SystemExit(__doc__)
    if series:
        pairs = [diff(args[i], args[i + 1]) for i in range(len(args) - 1)]
        out = {"adjacent": pairs, "first_vs_last": diff(args[0], args[-1])}
    else:
        out = diff(args[0], args[1])
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
