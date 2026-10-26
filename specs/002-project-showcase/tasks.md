# Tasks: DOOM 3 Browser Product Concept Deck

**Input**: Design documents from `/specs/002-project-showcase/`

**Prerequisites**: `spec.md`, `plan.md`

**Status legend**: `[x]` done · `[ ]` pending · `[~]` superseded / cancelled

## Phase 1 — Setup

- [x] T001 Create `showcase/src/`, `showcase/assets/`, `showcase/evidence/` and ignore runtime artifacts
- [x] T002 Author `showcase/src/narration.json` (three tracks, bilingual chunks, voice and rate)
- [x] T003 Author the shell: `head.html`, `base.css`, `posters.css`, `body.html`

## Phase 2 — Runtime foundation

- [x] T004 1920×1080 stage scaling + safe areas + poster transitions
- [x] T005 Page state: audio/silent clock, pause/mute/captions, query modes
- [x] T006 Caption engine: chunk cross-fade, word highlight, `?snap=1` settled state
- [x] T007 Auto-advance on narration end + 3→1 loop
- [x] T008 Autoplay-blocked fallback + one-click sound gate

## Phase 3 — Visual systems

- [x] T023 Procedural industrial reference frame (`runtime.frame.js`) as the no-media fallback
- [x] T028 Product-concept redesign: browser/monitor mockup, live HUD, capability cards
- [x] T029 `assets/hero-redacted.png`: real frame → 16:9 mosaic → full-frame `XX` grid → `REDACTED` stamp
- [x] T030 Default redacted hero, `--no-hero` procedural fallback, `--hero-label`, `--hero-credit`
- [x] T031 AI Rogue concept demo: prompt / seed / map / modifier + generate button + concept note
- [x] T032 Poster 3 vision cards (REMAP, MOD, AI ROGUE), loop ribbon and roadmap
- [~] T024 Frame/portal toggle — **superseded** by the product-concept monitor; removed

## Phase 4 — Build

- [x] T025 Edge TTS + word boundaries + MP3 duration + base64 data URIs
- [x] T026 Local TTS cache (`showcase/evidence/tts-cache.json`) for fast/offline rebuilds
- [x] T027 `build.py --hero` accepts image/video; `--out` supports temp builds
- [x] T028b Default build embeds the redacted hero; `--no-hero` produces the procedural deck

## Phase 5 — Presenter controls and evidence

- [x] T012 Prev/next, pause/resume, mute, captions, fullscreen; keyboard + swipe
- [x] T013 Evidence interactions: claim hover/focus and roadmap hover
- [x] T014 Sound gate resumes the current poster after a blocked autoplay
- [x] T015 `start.cmd`: Edge kiosk + autoplay exemption + local profile
- [x] T016 README: run, rebuild, redacted asset, licensed replacement, verification
- [x] T017 Assets provenance note (`assets/README.md`)

## Phase 6 — Verification

- [x] T018 `verify.py`: offline audit, three posters, console, audio, captions, claims, redacted hero,
  capability cards, AI Rogue, controls, fallback, screenshots
- [x] T019 `verify_deep.py`: Edge + Chrome, offline, reduced motion, **real CDP touch** (no `pointercancel`),
  Space-on-control, visibility, canvas fallback, loop, caption sync at all chunks, evidence, mute,
  fullscreen, 4K + DPR2, portrait, animation, performance, a11y names, HTML parse, `start.cmd`
- [x] T020 Human visual pass on all three settled screenshots; fixed HUD readability, URL class,
  card visuals and monitor bezel
- [x] T022 Licence audit: no legally reusable screenshot found; documented the redacted-placeholder
  decision and the rejected candidates
- [x] T033 Final build + `verify.py` **100 passed, 0 failed**
- [x] T034 Final `verify_deep.py` **150 passed, 0 failed, 1 skipped** (Firefox not installed)
- [x] T035 axe-core 4.10.3 WCAG 2.1 A/AA **0 violations**; 1 incomplete color-contrast/poster
  (119/88/111 nodes) recorded rather than claimed as full AA
- [x] T036 Contrast fixes: browser URL and AI Rogue concept note promoted from `--faint` to `--dim`
- [x] T037 Touch fix: hero media `pointer-events:none` + `draggable=false` + `#stage{touch-action:none}`
- [x] T038b Real CDP touch regression test (touchStart/move/end, no `pointercancel`)
- [x] T043 Space-on-control guard + page-visibility pause/resume
- [x] T044 Canvas-unavailable fallback + caption `aria-hidden` + `#ccEn lang="en"`
- [x] T045 Truthfulness: `M0 · SCAFFOLD`, future-tense lead, no false CI baseline, no “no retail assets” claim
- [x] T046 Byte-reproducibility (two builds, SHA-256 `62f7…96f8e`) + hero-label edge cases
- [x] T047 README/spec/plan/tasks + PR evidence updated to 100/150/axe-incomplete/56.3 fps
- [x] T048 Re-ran contract, deep and axe after the review fixes
- [x] T038 Animation test fix: canvas frame-change + 0.6% page diff threshold
- [x] T039 README/spec/plan/tasks aligned to the product concept

## Phase 7 — Ship and review

- [x] T021 Commit only scoped paths; push `feat/2-project-showcase`; open PR with evidence and `Closes #2`
- [ ] T040 Independent review of the PR diff against this spec (reviewer ≠ author)
- [ ] T041 Merge after review (squash only; coordinator/reviewer, not the author)
- [ ] T042 Replace `hero-redacted.png` with a licensed capture before public distribution
  (owner/licensing gate; not a code blocker for the PR)

## Dependencies

- T002/T003 → T004–T008 → T023/T028 → T029/T030/T031 → T025/T026 → T018/T019
- T018/T019 → T020 → T043–T048 → T021 → T040 → T041
- T042 is a release gate, not a PR blocker; the redacted asset is labelled pending.

## Completion

| Bucket | Count |
|---|---|
| Done | 43 |
| Superseded | 1 (T024) |
| Pending before review | 0 |
| Pending review/merge | 2 (T040, T041) |
| Pending release/licensing | 1 (T042) |

## Acceptance Mapping

| Spec | Tasks |
|---|---|
| FR-001, FR-002, FR-005, FR-018 | T004, T005, T008, T018, T019 |
| FR-003, FR-014 | T002, T006, T025 |
| FR-004, FR-006 | T004, T012 |
| FR-007, FR-008 | T013, T018, T020 |
| FR-009, FR-012 | T023, T029, T030, T018 |
| FR-010, FR-011 | T028, T031, T018, T019 |
| FR-013 | T025, T026, T027 |
| FR-015 | T015, T019 |
| FR-016, FR-017 | T018, T019, T033, T034 |
| SC-001 | T018, T033 |
| SC-002 | T018, T019 |
| SC-003 | T018 |
| SC-004 | T019, T033 |
| SC-005 | T012, T018 |
| SC-006 | T018 |
| SC-007 | T019 |
| SC-008 | T018 |
| SC-009 | T029, T030 |
| SC-010 | T031, T018 |
| SC-011 | T035, T036 |
