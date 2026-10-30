#!/usr/bin/env python3
"""Regenerate the T056 B-3..B-6 review crops from the committed frames.

Requires Pillow (12.3.0 on the authoring host, the same pin the content
toolchain records). The PNGs themselves are committed; this script only rebuilds
the 2x/4x nearest-neighbour comparisons so a reviewer can see the claim without
zooming the full frame. Run from the repository root:

    py -3 specs/004-quality-renderer/evidence/t056/tools/make_crops.py
"""
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
EVID = os.path.abspath(os.path.join(HERE, ".."))
CROPS = os.path.join(EVID, "crops")


def labelled_pair(left, right, label_left, label_right, rect, scale, name):
    a = Image.open(os.path.join(EVID, left)).convert("RGB").crop(rect)
    b = Image.open(os.path.join(EVID, right)).convert("RGB").crop(rect)
    w, h = a.size
    a = a.resize((w * scale, h * scale), Image.NEAREST)
    b = b.resize((w * scale, h * scale), Image.NEAREST)
    bar = 22
    canvas = Image.new("RGB", (a.width + b.width + 8, a.height + bar),
                       (18, 18, 20))
    canvas.paste(a, (0, bar))
    canvas.paste(b, (a.width + 8, bar))
    draw = ImageDraw.Draw(canvas)
    draw.text((4, 5), label_left, fill=(220, 230, 240))
    draw.text((a.width + 12, 5), label_right, fill=(220, 230, 240))
    canvas.save(os.path.join(CROPS, name))
    print("wrote", name, canvas.size)


def single(path, rect, scale, name, label):
    im = Image.open(os.path.join(EVID, path)).convert("RGB").crop(rect)
    w, h = im.size
    im = im.resize((w * scale, h * scale), Image.NEAREST)
    canvas = Image.new("RGB", (im.width, im.height + 20), (18, 18, 20))
    canvas.paste(im, (0, 20))
    ImageDraw.Draw(canvas).text((4, 4), label, fill=(220, 230, 240))
    canvas.save(os.path.join(CROPS, name))
    print("wrote", name, canvas.size)


def main():
    os.makedirs(CROPS, exist_ok=True)
    labelled_pair(
        "09-transmission-reference.png", "05-transmissive-environment.png",
        "reference (no glass)", "glass: thin-slab refraction",
        (500, 120, 760, 450), 2, "glass-refraction-2x.png")
    labelled_pair(
        "03-moving-frame-1.png", "04-moving-frame-8.png",
        "frame 1 (pose A)", "frame 8 (pose B)",
        (830, 140, 1030, 470), 2, "skinned-pose-2x.png")
    labelled_pair(
        "10-emissive-sdr-reference.png", "11-emissive-hdr-rolloff.png",
        "SDR, emissiveScale 1", "HDR, emissiveScale 2 (ACES rolloff)",
        (380, 180, 630, 420), 2, "emissive-hdr-rolloff-2x.png")
    single("08-hud-overlay-display.png", (16, 660, 256, 706), 4,
           "hud-texel-4x.png",
           "HUD at 1280x720 display after 960x540 scene upscale: 2 px border")


if __name__ == "__main__":
    main()
