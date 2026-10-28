# Feature Specification: AIGC Modern Vision for Posters 2 and 3

**Feature Branch**: `feat/2-aigc-vision`

**Issue**: #2 (kept OPEN; this PR only references it)

**Created**: 2026-10-28

**Status**: Implemented and self-verified 2026-10-28 — `verify.py` 157/0, `verify_deep.py` 182/0/1,
axe 0 violations, `verify_variants.py` 58/0, two byte-identical builds. Independent PR review and
merge (T012) plus the page-2 licensing gate (T042) remain.

## Context

The merged showcase (PR #3, 124/161) uses a procedural corridor on poster 2 and a procedural
network on poster 3. The owner wants the modern vision to be produced with **GPT Image 2.5**:

- Poster 2: image-to-image from the **official 2004 promo frame** (the original visual), producing a
  2026 modern-FPS reimagining.
- Poster 3: text-to-image, a Mars command center / holographic level map for the AI-native vision.
- Both images must be embedded in the single-file offline deck.

The base URL is `https://llm-proxy.tapsvc.com/v1`; the model is
`openai/gpt-image-2.5-flare`. Text-to-image (`/v1/images/generations`) and image-to-image
(`/v1/images/edits`) were both verified **HTTP 200 / b64_json**.

## Truth and licensing boundary

- The page-2 generated image is a **derivative** of the official DOOM 3 promo screenshot
  (© id/Bethesda). The original is **not** committed. The generated image is a concept vision, not a
  game capture; licensing remains pending and issue #2 stays open for T042.
- The page-3 generated image is **text-to-image** with no game-art input; it is original
  AI-generated concept art.
- Both images are labelled `AI-GENERATED VISION · NOT A GAME CAPTURE`.
- The original promo and the unredacted input remain outside git.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — The modern vision looks like a real 2026 frame (P1)

A visitor sees poster 2's 2004 redacted archival on the left and a GPT Image 2.5 modern reimagining on
the right; the diagonal split, drag, reveal buttons and damped auto-sweep still work.

**Independent Test**: Poster 2 contains the generated image, the redacted archival, the split, the
labels; the reveal/auto controls pass.

### User Story 2 — The vision page has a real command-center backdrop (P2)

Poster 3 uses the generated Mars command center as a background while the three capability cards,
the loop and the roadmap stay readable.

**Independent Test**: Poster 3 contains the generated background, the `AI VISION · GPT IMAGE 2.5`
label, and all content remains legible.

### User Story 3 — A maintainer can regenerate or fall back (P3)

The build embeds the optimized JPEGs by default; `--no-aigc` restores the procedural corridor and
network. The prompts and provenance live in `showcase/assets/aigc/README.md`.

## Requirements

- **FR-001**: The deck MUST default to the two GPT Image 2.5 JPEGs, embedded as data URIs.
- **FR-002**: Page 2 MUST keep the redacted 2004 archival on the left and the AIGC modern vision on
  the right, with the diagonal split, drag knob, REVEAL/BACK buttons and 2.6 s idle auto-sweep.
- **FR-003**: Page 3 MUST use the AIGC command-center image as a background while the REMAP/MOD/AI
  ROGUE cards, loop and roadmap remain readable.
- **FR-004**: Both pages MUST be labelled `AI-GENERATED VISION · NOT A GAME CAPTURE`; page 3 MUST
  show `AI VISION · GPT IMAGE 2.5`.
- **FR-005**: The provenance note MUST record model, base URL, endpoint, prompt, input and the
  derivative/licensing status; the original input MUST NOT be committed.
- **FR-006**: `build.py` MUST support `--no-aigc` (procedural fallback) and MUST reuse the default
  redacted hero for the page-2 left side instead of embedding it twice.
- **FR-007**: The result MUST remain one offline file under 4 MiB with no external subresource.
- **FR-008**: `verify.py` and `verify_deep.py` MUST assert the AIGC images/labels, the controls, the
  page 3 content, the offline/size constraints, and produce three distinct screenshots.

## Success Criteria

- **SC-001**: `index.html` is 100 KB–4 MiB, contains two `data:image/jpeg` AIGC assets, and makes no
  external request.
- **SC-002**: `verify.py` and `verify_deep.py` pass; axe remains 0 violations with the known
  incomplete color-contrast nodes recorded.
- **SC-003**: Page 2 and page 3 screenshots are non-blank, distinct, and show the AIGC imagery.
- **SC-004**: Page 2's split stays functional: REVEAL 32 / BACK 80, drag, auto 72/40, snap.
- **SC-005**: Page 3's cards/loop/roadmap are readable at 1920 and 4K.
- **SC-006**: `--no-aigc` produces a working procedural fallback from a temp build.

## Assumptions

- The owner accepts the page-2 generated derivative for this concept deck; T042 licensing remains.
- The images are 1600×900 JPEGs (upscaled to the 1920 stage) to stay under the 4 MiB budget.
- The original promo input is a local file only; no source image enters git.
