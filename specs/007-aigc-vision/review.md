# Self-review — AIGC Modern Vision (007)

**Date**: 2026-10-28
**Reviewer**: author self-review + two independent read-only subagent audits (verifier coverage and
spec-vs-code), plus a post-merge independent review of PR #23. The post-merge review returned FAIL
on evidence strength, not on the deck; its findings are fixed below.
**Artifact**: `showcase/index.html`, 2,741,798 bytes, SHA-256
`b758b90a9cd2bc1a98af0b6cae853a9f21348156d2a2281e30032f043e864afc`.

## Method

1. Read the spec, plan, tasks, build script, DOM, CSS, runtime and verifiers.
2. Rebuilt from source (TTS cache; no network), then ran the contract and deep matrices.
3. Ran `verify_axe.py` (axe-core 4.10.3) on all three settled posters.
4. Added `verify_variants.py`, which rebuilds the default twice plus `--no-aigc`, `--no-hero` and
   `--hero`, then checks them with Playwright.
5. Built twice to temp and compared SHA-256; captured 1920 and 4K screenshots and inspected them.
6. Ran two independent read-only subagent audits (verifier coverage and spec-vs-code).
7. Merged #23/#24, then ran the independent post-merge review; fixed the evidence gaps it found.

## Result matrix

| Requirement | Result | Evidence |
|---|---|---|
| FR-001 default embeds both images | PASS | exactly two JPEG data URIs; `#p2New`/`#p3Bg` exact URI; decoded 1600×900 |
| FR-002 page 2 split/drag/auto | PASS | 46→80→32 visual `--split` and clip change; pointer drag 32→54; auto 72→40 |
| FR-003 page 3 background + readable cards | PASS | `#p3Bg` decoded; three cards, seven loop steps, eight milestones; `elementFromPoint` proves cards are above the image at 1920 and 4K |
| FR-004 truth labels | PASS (after fix) | page 2 exact `AI-GENERATED VISION · NOT A GAME CAPTURE`; page 3 now carries the same truth label **and** `AI VISION · GPT IMAGE 2.5` |
| FR-005 provenance | PASS | `assets/aigc/README.md` records model, base URL, endpoints, prompts, input status, derivative/licensing status; verifier checks it |
| FR-006 `--no-aigc` + single PNG | PASS | one PNG data URI; `--no-aigc` build has zero JPEG data, hidden images, procedural labels/canvases, working split |
| FR-007 offline, <4 MiB | PASS | 2.61 MiB; zero external requests on every page; static src/href/srcset/poster audit |
| FR-008 verifier coverage | PASS (hardened) | exact URI, decoded size, labels, visual clip, drag, auto both sides, 4K pages 2/3, offline/size |
| SC-001–SC-006 | PASS | see the command results below |

## Defects found by the review and fixed

1. **FR-004 violation — page 3 had no truth label.** The old `AI VISION · GPT IMAGE 2.5` line was
   the only page-3 credit; the `verify.py` literal search ran document-wide on page 2 and could not
   see the omission. Added `#p3Truth` (`AI-GENERATED VISION · NOT A GAME CAPTURE`) to the page-3
   meta, a procedural variant for `--no-aigc`, and exact per-page assertions.
2. **Verifiers were green on broken output.** They only checked `startsWith('data:image/jpeg')`,
   substring labels, and `aria-valuenow`; a 1×1 image, a swapped asset, a dead clip-path or a
   missing page-3 truth label could pass. Hardened with exact data URIs, decoded dimensions,
   `data-aigc`, hidden procedural canvases, clip changes, pointer drag, auto 72→40, card stacking,
   single PNG embed, alt text and the AIGC provenance note.
3. **4K coverage was poster 1 only.** Added 4K posters 2/3, content-above-background checks and
   screenshots.
4. **No reproducible variant coverage.** Built and tested `--no-aigc`, `--no-hero` and `--hero`
   (58/58 checks), then committed the gate as `verify_variants.py`. The `--hero` path keeps the
   redacted archival on page 2 and does not reuse the licensed image there.
5. **`--no-aigc`/no-JS labels were false.** The static DOM still said `AI-GENERATED VISION` and
   `AI VISION · GPT IMAGE 2.5` until JS booted. `build.py` now substitutes procedural labels when
   no AIGC data is embedded, and both AIGC `<img>` elements start `hidden`.
6. **The axe claim had no runner.** Added `verify_axe.py` (axe-core not vendored; `AXE_PATH`) and
   saved `evidence/axe.json`.
7. **`preview.png` was stale.** Regenerated the 3-up from the current poster screenshots.
8. **The post-merge review found the visibility gate was not a visibility check.** `!hidden`,
   `complete` and 1600×900 still pass `display:none`, `visibility:hidden`, `opacity:0` or an opaque
   cover. Added a computed-style/rect predicate, a screenshot/asset Pearson correlation, and
   negative controls (`display:none`, `visibility:hidden`, `opacity:0`; hidden correlation must be
   <0.3). Measured: page 2 corr 0.998, page 3 corr 0.970; hidden 0.05/0.00.
9. **Licensing wording contradicted the public merge.** The deck is public at the owner's direction;
   the docs now say the owner accepted the risk, licensing is still pending, and it is not a
   licensed capture—instead of claiming it must not be published.
10. **Doc/evidence nits.** Removed the README/spec BOM, corrected the fps claim to 60.4–60.5,
    extended the FR-005 check to the base URL and input, and asserted the `--no-aigc` page-2 archival
    asset and visibility.

## Command evidence

| Command | Result |
|---|---|
| `uv run --python 3.12 showcase/build.py` | 2,677.5 KiB (2.61 MiB), 82.8 s narration, both AIGC images inlined, redacted hero reused |
| `uv run --python 3.12 showcase/verify.py` | **171 passed, 0 failed** |
| `uv run --python 3.12 showcase/verify_deep.py` | **186 passed, 0 failed, 1 skipped** (Firefox absent) |
| `verify_axe.py` (axe-core 4.10.3, WCAG 2.1 A/AA) | **0 violations**; 1 incomplete color-contrast per poster (120/66/113 nodes) |
| `verify_variants.py` | **63 passed, 0 failed** (default, `--no-aigc`, `--no-hero`, `--hero`) |
| reproducibility | `default-a` = `default-b` = `index.html` = `b758b90a…864afc`, 2,741,798 bytes |
| screenshots | `deep-edge-poster-2/3.png`, `deep-4k-poster-2/3.png`; visual check: AIGC imagery present, cards/loop/roadmap readable |

## Residual issues / not fixed

- **Licensing (T042, issue #2)** — page 2 is a derivative of the official promo. It is now in the
  public repo at the owner's direction; licensing remains pending and it is **not** a licensed
  capture. Replace it if a cleared distribution is required. This is a release/legal gate, not a
  code defect. Keep issue #2 open; the PRs use `Refs #2`.
- **Page-2 seam** — the redacted `REDACTED` stamp is partially visible at the diagonal when the split
  is near 46% (the letters `R` / `LICE`). It is truthful archival content, but visually it reads as
  stray text. Not a spec violation; a future polish could crop/fade the stamp on the comparison page.
- **Independent review / merge (T012)** — #23 and #24 are merged; no formal GitHub approving
  review is recorded (`reviewDecision` empty). The merge was owner-directed after two subagent
  audits; the post-merge reviewer found the evidence gaps fixed in this follow-up. A formal review
  record is still a process gap, not a code gap.
- **`--no-hero` scope** — it removes the poster-1 hero only; page 2 keeps the redacted archival
  inset (the flag's help text now says so). A fully redacted-free build would need a separate
  page-2 fallback and is not provided by this flag.
- **Cold-cache reproducibility** — the byte-identical result depends on the git-ignored TTS cache.
  A cold clone needs network TTS and is not guaranteed to match byte-for-byte.

## Verdict

Implementation is complete against FR-001..FR-008 and SC-001..SC-006 and is merged into `main`
(#23, #24). The post-merge evidence review's blocking findings are fixed in this follow-up: the
visibility gate now combines computed style, decoded pixels and negative controls, and the licensing
wording no longer contradicts the public state. T042 (page-2 licensing) is the only open delivery
gate; a formal GitHub approving review record is still absent.
