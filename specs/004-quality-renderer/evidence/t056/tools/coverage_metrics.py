#!/usr/bin/env python3
"""Coverage metrics for the T056 frames added by the B-3..B-6 session.

These numbers are recomputable from the committed PNGs (standard library only,
like defect_metrics.py). Every metric states its region and its definition:

Glass (05 vs 09, the no-transmission reference)
  region_xyxy      inner pane rectangle, chosen inside the pane's silhouette
  corr             Pearson correlation of luminance in the region
  mean_abs         mean |glass - reference| luminance, 0..255
  visible_fraction fraction of pixels with |delta| <= 20 (background survives)
  variance_ratio   var(glass) / var(reference)
  best_shift_px    horizontal offset dx in [-24,24] minimising mean |G[x]-R[x+dx]|
  sad_zero / sad_best / improvement_pct   the same quantity at dx=0 and at dx=best

Emissive (02 HDR scale 16 vs 10 SDR scale 1)
  region_xyxy      the sign panel
  mask_pixels      pixels whose SDR luminance is > 180 (the emissive glyph mask)
  sdr_mean / hdr_mean / sdr_std / hdr_std   luminance over that mask
  sdr_saturated / hdr_saturated             count at >= 250 in the same mask
  The scene-target radiance probe (peak, pixels_gt_1/8/32) is printed by the
  harness with -HdrProbe and recorded in measurements.json; this script only
  reads the 8-bit captures.

HUD (08)
  extent           display extent of the frame
  health_row_runs  cyan runs along the health bar's mid row at y = height-41
  border_width_px  width of the left/right plate borders (2 = exact pixels)
  hud_bbox_xyxy    bounding box of the cyan mask over the whole frame

Skinned (03 vs 04)
  region_xyxy      character-only rectangle (excludes the rigid cube)
  changed_px       pixels with max-channel delta > 2 inside the region
  mean_abs / max_abs / bbox
  outside_changed_px   the same threshold outside the region (the rotating cube)

WARP parity (01 vs 06, 05 vs 07)
  mean_abs, max_abs, changed_pct (>2)
"""
import json
import os
import sys

import defect_metrics as dm

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
EVID = os.path.join(ROOT, "specs", "004-quality-renderer", "evidence", "t056")

GLASS_INNER = (540, 190, 700, 330)
SIGN_REGION = (400, 190, 615, 410)
CHARACTER_REGION = (850, 150, 1020, 470)


def load(path):
    w, h, channels, rows = dm.read_png(path)
    out = []
    for y in range(h):
        row = rows[y]
        out.append([
            (row[x * channels], row[x * channels + 1], row[x * channels + 2])
            for x in range(w)
        ])
    return w, h, out


def lum(pixel):
    return 0.299 * pixel[0] + 0.587 * pixel[1] + 0.114 * pixel[2]


def pearson(a, b):
    n = len(a)
    ma = sum(a) / n
    mb = sum(b) / n
    cov = sum((x - ma) * (y - mb) for x, y in zip(a, b))
    va = sum((x - ma) ** 2 for x in a)
    vb = sum((y - mb) ** 2 for y in b)
    if va <= 0 or vb <= 0:
        return 0.0
    return cov / (va ** 0.5 * vb ** 0.5)


def region_values(grid, rect):
    x0, y0, x1, y1 = rect
    return [lum(grid[y][x]) for y in range(y0, y1) for x in range(x0, x1)]


def max_channel_delta(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2]))


def diff_stats(a, b, rect=None, threshold=2):
    h = min(len(a), len(b))
    w = min(len(a[0]), len(b[0]))
    x0, y0, x1, y1 = rect if rect else (0, 0, w, h)
    total = count = 0
    changed = 0
    maximum = 0
    bx0, by0, bx1, by1 = w, h, -1, -1
    for y in range(y0, y1):
        for x in range(x0, x1):
            d = max_channel_delta(a[y][x], b[y][x])
            total += d
            count += 1
            maximum = max(maximum, d)
            if d > threshold:
                changed += 1
                bx0, by0 = min(bx0, x), min(by0, y)
                bx1, by1 = max(bx1, x), max(by1, y)
    return {
        "mean_abs": round(total / count, 3) if count else 0.0,
        "max_abs": maximum,
        "changed_px": changed,
        "changed_pct": round(100.0 * changed / count, 3) if count else 0.0,
        "changed_bbox_xyxy": [bx0, by0, bx1, by1] if bx1 >= 0 else None,
    }


def glass_metrics(glass_path, reference_path):
    w, h, g = load(glass_path)
    _, _, r = load(reference_path)
    x0, y0, x1, y1 = GLASS_INNER
    gl = region_values(g, GLASS_INNER)
    rl = region_values(r, GLASS_INNER)
    deltas = [abs(a - b) for a, b in zip(gl, rl)]
    mean_g = sum(gl) / len(gl)
    mean_r = sum(rl) / len(rl)
    var_g = sum((v - mean_g) ** 2 for v in gl) / len(gl)
    var_r = sum((v - mean_r) ** 2 for v in rl) / len(rl)
    # Horizontal refraction shift: compare G[y][x] with R[y][x + dx].
    shifts = {}
    for dx in range(-24, 25):
        total = 0
        count = 0
        for y in range(y0 + 20, y1 - 10, 4):
            for x in range(x0, x1, 2):
                sx = x + dx
                if x0 - 24 <= sx < w:
                    total += abs(lum(g[y][x]) - lum(r[y][sx]))
                    count += 1
        shifts[dx] = total / count
    best = min(shifts, key=shifts.get)
    sad_zero = shifts[0]
    return {
        "glass": os.path.basename(glass_path),
        "reference": os.path.basename(reference_path),
        "region_xyxy": list(GLASS_INNER),
        "corr": round(pearson(gl, rl), 4),
        "mean_abs": round(sum(deltas) / len(deltas), 3),
        "visible_fraction": round(
            sum(1 for d in deltas if d <= 20) / len(deltas), 4),
        "variance_ratio": round(var_g / var_r, 4) if var_r else None,
        "best_shift_px": best,
        "sad_zero": round(sad_zero, 3),
        "sad_best": round(shifts[best], 3),
        "improvement_pct": round(100.0 * (sad_zero - shifts[best]) / sad_zero, 2),
    }


def emissive_metrics(hdr_path, sdr_path):
    w, h, hdr = load(hdr_path)
    _, _, sdr = load(sdr_path)
    x0, y0, x1, y1 = SIGN_REGION
    mask = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            if lum(sdr[y][x]) > 180.0:
                mask.append((x, y))
    sdr_values = [lum(sdr[y][x]) for x, y in mask]
    hdr_values = [lum(hdr[y][x]) for x, y in mask]

    def stats(values):
        n = len(values)
        mean = sum(values) / n if n else 0.0
        var = sum((v - mean) ** 2 for v in values) / n if n else 0.0
        return round(mean, 3), round(var ** 0.5, 3)

    sdr_mean, sdr_std = stats(sdr_values)
    hdr_mean, hdr_std = stats(hdr_values)
    return {
        "hdr": os.path.basename(hdr_path),
        "sdr": os.path.basename(sdr_path),
        "region_xyxy": list(SIGN_REGION),
        "mask_definition": "SDR luminance > 180",
        "mask_pixels": len(mask),
        "sdr_mean": sdr_mean,
        "hdr_mean": hdr_mean,
        "sdr_std": sdr_std,
        "hdr_std": hdr_std,
        "sdr_saturated": sum(1 for v in sdr_values if v >= 250.0),
        "hdr_saturated": sum(1 for v in hdr_values if v >= 250.0),
    }


def hud_metrics(path):
    w, h, grid = load(path)
    row = h - 41
    runs = []
    start = None
    for x in range(w):
        pixel = grid[row][x]
        is_cyan = pixel[1] > 150 and pixel[2] > 180 and pixel[0] < 160
        if is_cyan and start is None:
            start = x
        elif not is_cyan and start is not None:
            runs.append([start, x - 1])
            start = None
    if start is not None:
        runs.append([start, w - 1])
    border = runs[0] if runs else None
    bbox = [w, h, -1, -1]
    for y in range(h):
        for x in range(w):
            pixel = grid[y][x]
            if pixel[1] > 150 and pixel[2] > 180 and pixel[0] < 160:
                bbox[0] = min(bbox[0], x)
                bbox[1] = min(bbox[1], y)
                bbox[2] = max(bbox[2], x)
                bbox[3] = max(bbox[3], y)
    return {
        "frame": os.path.basename(path),
        "extent": [w, h],
        "health_mid_row_y": row,
        "health_row_runs": runs,
        "left_border_width_px": (border[1] - border[0] + 1) if border else None,
        "hud_bbox_xyxy": bbox if bbox[2] >= 0 else None,
        "note": "runs are exact cyan-pixel intervals on one display row; a "
                "2-pixel border followed by a gap is the texel-exact overlay "
                "signature of a pass that is not upscaled",
    }


def skinned_metrics(first_path, last_path):
    w, h, first = load(first_path)
    _, _, last = load(last_path)
    inside = diff_stats(first, last, CHARACTER_REGION)
    outside = diff_stats(first, last, (0, 0, w, h))
    outside_changed = outside["changed_px"] - inside["changed_px"]
    return {
        "first": os.path.basename(first_path),
        "last": os.path.basename(last_path),
        "region_xyxy": list(CHARACTER_REGION),
        "inside": inside,
        "outside_changed_px": outside_changed,
        "outside_changed_pct": round(100.0 * outside_changed / (w * h), 3),
    }


def main():
    def p(name):
        return os.path.join(EVID, name)

    _, _, static_hw = load(p("01-static-opaque-dielectric-metal.png"))
    _, _, static_warp = load(p("06-warp-static-opaque.png"))
    _, _, glass_hw = load(p("05-transmissive-environment.png"))
    _, _, glass_warp = load(p("07-warp-transmissive-environment.png"))
    result = {
        "glass_transmission": glass_metrics(
            p("05-transmissive-environment.png"),
            p("09-transmission-reference.png")),
        "emissive_hdr": emissive_metrics(
            p("02-alpha-tested-emissive.png"),
            p("10-emissive-sdr-reference.png")),
        "emissive_rolloff_diagnostic": emissive_metrics(
            p("11-emissive-hdr-rolloff.png"),
            p("10-emissive-sdr-reference.png")),
        "hud_display_overlay": hud_metrics(p("08-hud-overlay-display.png")),
        "skinned_motion": skinned_metrics(
            p("03-moving-frame-1.png"), p("04-moving-frame-8.png")),
        "warp_parity": {
            "static_opaque": diff_stats(static_hw, static_warp),
            "transmissive_environment": diff_stats(glass_hw, glass_warp),
        },
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
