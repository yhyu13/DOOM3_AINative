#!/usr/bin/env python3
"""Recompute the B1/B2 numbers in ../measurements.json from the committed PNGs.

Standard library only, so a reviewer can run it with `py -3` and nothing else.
Every number it prints comes from a PNG in this directory or from the rejected
frame in git history, so it is reproducible without rebuilding the harness.

Examples
--------
B2, dielectric surface response (static and moving frames):

  py -3 defect_metrics.py --only b2 --profile static 01-static-opaque-dielectric-metal.png
  py -3 defect_metrics.py --only b2 --profile moving 03-moving-frame-1.png 04-moving-frame-8.png

B1, cutout shadow topology, using the rejected frame as the footprint source
(the rejected frame is in git history, not in the working tree):

  git show 50efd1a:specs/004-quality-renderer/evidence/t056/02-alpha-tested-emissive.png > old-02.png
  py -3 defect_metrics.py --only b1 --mask-from old-02.png 02-alpha-tested-emissive.png

Definitions
-----------
B2. Inside a fixed ROI of the frame:
  * `highpass_sd` = standard deviation of (pixel - 3x3 mean) luminance. It is the
    energy the surface carries above its own local shading gradient. A painted
    panel sits near 0.5-1.0; per-texel normal/albedo noise is 10-20.
  * `isolated_spikes` = pixels that are bright in absolute terms (>= 200/255) and
    brighter than their 8-neighbour mean by >= 25/255. That is the "isolated
    white dot" the T057 review rejected; a smooth highlight never triggers it.
  ROIs were checked against 4x crops of both the rejected and the accepted frame,
  so they sit on the named surface and off silhouettes and shadow edges.

B1. The footprint is the solid shadow of the rejected frame: within the region
that contains only that caster's shadow, the frame is 3x3 median filtered, the
5th and 95th percentile of the region give a threshold at their midpoint, the
largest 8-connected dark component is the shadow, and that mask is eroded by 2
pixels to stay off its edges. The same mask is then applied to the accepted
frame, where the question is whether the shadow is still one solid block or has
been broken into pieces by light through the cutout's holes:
  * `dark_components` = 8-connected components of (mask and median < 160).
    A solid shadow is 1; a perforated one is several.
  * `sd` / `range` = spread of the median-filtered luminance inside the mask.
  * `lit_share` = share of mask pixels at or above (region p95 - 4), i.e. holes
    that come back close to unshadowed ground.
"""
import json
import math
import struct
import sys
import zlib


def read_png(path):
    """Decode an 8-bit RGB/RGBA PNG into rows of raw channel bytes."""
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(path + ": not a PNG")
    pos, idat, w, h, ctype = 8, bytearray(), None, None, None
    while pos < len(data):
        (ln,) = struct.unpack(">I", data[pos:pos + 4])
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        pos += 12 + ln
        if tag == b"IHDR":
            w, h, _bit, ctype = struct.unpack(">IIBB", body[:10])
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
    raw = zlib.decompress(bytes(idat))
    channels = {2: 3, 6: 4, 0: 1}[ctype]
    stride = w * channels
    rows = []
    prev = bytearray(stride)
    p = 0
    for _y in range(h):
        f = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = prev[i]
            c = prev[i - channels] if i >= channels else 0
            x = line[i]
            if f == 1:
                x = (x + a) & 0xFF
            elif f == 2:
                x = (x + b) & 0xFF
            elif f == 3:
                x = (x + ((a + b) >> 1)) & 0xFF
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                x = (x + pr) & 0xFF
            line[i] = x
        rows.append(line)
        prev = line
    return w, h, channels, rows


STATIC_BOXES = [
    ("dielectric_sphere", 300, 300, 140, 130),
    ("dielectric_cube_face", 545, 275, 190, 170),
    ("ground_near_left", 30, 620, 300, 80),
    ("ground_mid_right", 980, 480, 260, 120),
]
MOVING_BOXES = [
    ("dielectric_sphere", 300, 300, 140, 130),
    ("ground_near_left", 30, 620, 300, 80),
    ("ground_mid_right", 980, 480, 260, 120),
]
B1_REGION = (590, 395, 240, 56)
B1_DARK_LEVEL = 160


def roi_stats(grid, x0, y0, w, h, bright=200, spike=25):
    values = [v for y in range(y0, y0 + h) for v in grid[y][x0:x0 + w]]
    n = len(values)
    mean = sum(values) / n
    var = sum((v - mean) ** 2 for v in values) / n
    resid = []
    dots = 0
    for y in range(y0 + 1, y0 + h - 1):
        for x in range(x0 + 1, x0 + w - 1):
            acc = 0
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    acc += grid[y + dy][x + dx]
            local = acc / 9.0
            v = grid[y][x]
            resid.append(v - local)
            if v >= bright and v - local >= spike:
                dots += 1
    rmean = sum(resid) / len(resid)
    rvar = sum((r - rmean) ** 2 for r in resid) / len(resid)
    mags = sorted(abs(r) for r in resid)
    return {
        "mean": round(mean, 3),
        "sd": round(math.sqrt(var), 3),
        "min": min(values),
        "max": max(values),
        "highpass_sd": round(math.sqrt(rvar), 3),
        "highpass_abs_p99": round(mags[int(len(mags) * 0.99)], 3),
        "highpass_abs_max": round(mags[-1], 3),
        "isolated_spikes": dots,
        "isolated_spikes_per_kpx": round(dots * 1000.0 / len(resid), 2),
        "pixels": n,
    }


def median3(grid, x0, y0, w, h):
    out = [[0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            vals = []
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < h and 0 <= xx < w:
                        vals.append(grid[y0 + yy][x0 + xx])
            vals.sort()
            out[y][x] = vals[len(vals) // 2]
    return out


def components(mask, w, h):
    seen = [[False] * w for _ in range(h)]
    comps = []
    for y in range(h):
        for x in range(w):
            if mask[y][x] and not seen[y][x]:
                stack = [(x, y)]
                seen[y][x] = True
                pts = []
                while stack:
                    cx, cy = stack.pop()
                    pts.append((cx, cy))
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            nx, ny = cx + dx, cy + dy
                            if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] and mask[ny][nx]:
                                seen[ny][nx] = True
                                stack.append((nx, ny))
                comps.append(pts)
    comps.sort(key=len, reverse=True)
    return comps


def erode(mask, w, h, r):
    out = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if not mask[y][x]:
                continue
            ok = True
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < h and 0 <= xx < w and not mask[yy][xx]:
                        ok = False
            out[y][x] = ok
    return out


def load(path):
    w, h, channels, px = read_png(path)
    grid = [[0] * w for _ in range(h)]
    for y in range(h):
        row, target = px[y], grid[y]
        for x in range(w):
            base = x * channels
            target[x] = int(0.2126 * row[base] + 0.7152 * row[base + 1] + 0.0722 * row[base + 2] + 0.5)
    return w, h, grid


def shadow_mask(rejected):
    _w, _h, grid = load(rejected)
    x0, y0, bw, bh = B1_REGION
    med = median3(grid, x0, y0, bw, bh)
    flat = sorted(v for row in med for v in row)
    p5 = flat[int(len(flat) * 0.05)]
    p95 = flat[int(len(flat) * 0.95)]
    thr = (p5 + p95) / 2.0
    dark = [[med[y][x] < thr for x in range(bw)] for y in range(bh)]
    comps = components(dark, bw, bh)
    if not comps:
        raise SystemExit(rejected + ": no shadow found in the B1 region")
    mask = [[False] * bw for _ in range(bh)]
    for x, y in comps[0]:
        mask[y][x] = True
    return erode(mask, bw, bh, 2), bw, bh, thr, p95


def b1_report(rejected, accepted):
    mask, bw, bh, thr, p95 = shadow_mask(rejected)
    n = sum(row.count(True) for row in mask)
    lit_level = p95 - 4
    rows = []
    for path in (rejected, accepted):
        _w, _h, grid = load(path)
        x0, y0 = B1_REGION[0], B1_REGION[1]
        med = median3(grid, x0, y0, bw, bh)
        vals = [med[y][x] for y in range(bh) for x in range(bw) if mask[y][x]]
        mean = sum(vals) / len(vals)
        sd = math.sqrt(sum((v - mean) ** 2 for v in vals) / len(vals))
        dark = [[mask[y][x] and med[y][x] < B1_DARK_LEVEL for x in range(bw)] for y in range(bh)]
        comps = [c for c in components(dark, bw, bh) if len(c) >= 3]
        lit = sum(1 for v in vals if v >= lit_level)
        rows.append({
            "frame": path,
            "footprint_px": n,
            "mask_threshold": round(thr, 1),
            "median_based_mean": round(mean, 2),
            "sd": round(sd, 2),
            "min": min(vals),
            "max": max(vals),
            "dark_components": len(comps),
            "dark_component_sizes": sorted((len(c) for c in comps), reverse=True)[:12],
            "lit_share": round(lit / float(n), 4),
            "lit_level": lit_level,
        })
    return rows


def main():
    args = sys.argv[1:]
    only = None
    profile = "static"
    mask_from = None
    frames = []
    i = 0
    while i < len(args):
        if args[i] == "--only":
            only = args[i + 1]
            i += 2
        elif args[i] == "--profile":
            profile = args[i + 1]
            i += 2
        elif args[i] == "--mask-from":
            mask_from = args[i + 1]
            i += 2
        else:
            frames.append(args[i])
            i += 1
    if not frames:
        raise SystemExit(__doc__)

    if only == "b1":
        if mask_from is None or len(frames) != 1:
            raise SystemExit("--only b1 needs --mask-from <rejected.png> <accepted.png>")
        print(json.dumps({"b1": b1_report(mask_from, frames[0])}, indent=2))
        return

    boxes = STATIC_BOXES if profile == "static" else MOVING_BOXES
    for path in frames:
        _w, _h, grid = load(path)
        print(json.dumps({"frame": path, "profile": profile,
                          "b2": {name: roi_stats(grid, x, y, bw, bh)
                                 for name, x, y, bw, bh in boxes}}, indent=2))


if __name__ == "__main__":
    main()
