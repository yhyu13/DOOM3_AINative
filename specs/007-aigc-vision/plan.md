# Implementation Plan: AIGC Modern Vision

**Branch**: `feat/2-aigc-vision` | **Issue**: #2 (open) | **Spec**: [spec.md](./spec.md)

## Summary

Generate two GPT Image 2.5 assets, compress them to 1600×900 JPEG, embed them in the single-file
deck, and wire them into the existing poster 2 comparison and poster 3 vision without changing the
controls. Default to AIGC; keep `--no-aigc` as a procedural fallback. Keep the old issue open for
the T042 licensing gate.

## Technical Context

- Model: `openai/gpt-image-2.5-flare` at `https://llm-proxy.tapsvc.com/v1`.
- Endpoints: `POST /v1/images/generations` (page 3), `POST /v1/images/edits` (page 2, original input).
- Build: Python 3.12 + Pillow for 16:9 crop / 1600×900 JPEG q86; `build.py` base64 inline.
- Artifact: one HTML, offline `file://`, under 4 MiB.
- Verification: existing `verify.py` / `verify_deep.py`, axe, screenshots.

## Asset decisions

1. **Page 2** — image-to-image from the local official promo screenshot (1024×768), preserving the
   imp / corridor / shotgun composition, modern PBR / volumetric / HDR treatment. Output 1536×1024;
   crop 16:9 and resize to 1600×900 JPEG. The original is never committed.
2. **Page 3** — text-to-image of a Mars command center / holographic level map / prompt console;
   crop 16:9 and resize to 1600×900 JPEG. No game-art input.
3. Provenance in `showcase/assets/aigc/README.md`: model, base URL, endpoint, prompt, input status,
   licensing. Page 2 is a derivative; page 3 is text-only.
4. `build.py` embeds the JPEGs as `data:aigc.page2/page3`; `--no-aigc` leaves them null.
5. Reuse the default redacted hero for the page-2 left side (`DATA.hero.src`), so the redacted asset
   is not embedded twice; a licensed `--hero` still gets a separate redacted archival for page 2.

## UI decisions

- Page 2: add `#p2New` full-bleed behind the old clipped image; hide `#fx2` when AIGC; keep the
  split/auto/labels. The vision label reads `AI-GENERATED VISION · NOT A GAME CAPTURE`.
- Page 3: add `#p3Bg` full-bleed behind the cards; hide `#fx3` when AIGC; strengthen the shade so the
  cards/loop/roadmap stay readable. Add `AI VISION · GPT IMAGE 2.5` to the page meta.
- Aspect: 1600×900 JPEGs, `object-fit:cover`, no external font/image.

## Verification

- Update `verify.py`: page 2 `#p2New` data JPEG + AIGC label; page 3 `#p3Bg` + AI meta; size <4 MiB.
- Update `verify_deep.py`: image presence, labels, page 2 controls, page 3 content, 4K readability,
  screenshots, axe.
- Temp `--no-aigc` build: verify procedural fallback and no AIGC data.
- Two default builds byte-identical.

## Licensing / issue

The page-2 image remains a derivative of the official promo; issue #2 stays **open** for T042. The
PR body uses `Refs #2`, never `Closes`. No original image enters git.
