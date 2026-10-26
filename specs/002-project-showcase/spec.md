# Feature Specification: DOOM 3 Browser Product Concept Deck

**Feature Branch**: `feat/2-project-showcase`

**Created**: 2026-10-26

**Revised**: 2026-10-26 — product-concept direction (browser monitor, capability cards, AI Rogue demo, redacted real frame)

**Status**: Implemented; 110 contract checks + 158 deep checks + axe 0 violations (1 incomplete color-contrast/poster); pending review

**Input**: User description: "Write an attractive single interactive HTML with voice-over, using a skill, to show this project to others. Three rotating poster pages. Use a real DOOM 3 frame inside a browser/monitor, plus marketing concepts like key remapping, mods, and one-sentence AI Rogue generation. Follow the agent workflow. Use subagents in parallel."

## Context and truth boundary

The repository is a scaffold: pristine idTech4 source, no game data, no playable web build. The deck
is a **product concept**, not a launch claim. It shows what the product is meant to become and
separates **shipped** from **planned** and **concept**.

A licence audit on 2026-10-26 found no legally reusable DOOM 3 gameplay screenshot: id's GPL release
is source only, and every online candidate was fair use, a non-screenshot, or a non-GPL-compatible CC
licence. The owner explicitly chose a **redacted real frame** for the submitted version:

- Source: official DOOM 3 promotional screenshot from the Steam store (app 9050), © id/Bethesda.
- Transformation: 16:9 crop, 48×27 mosaic, ember grade, full-frame `XX` grid, `REDACTED` bar,
  scanlines, vignette.
- The original is not in the repository. The redacted asset is `LICENSING PENDING` and is a
  placeholder; it must be replaced with a licensed capture before public distribution.

The six quantitative evidence claims live in README/`docs/ROADMAP.md` §1, captured 2026-10-26; poster 2 carries a one-line proof strip instead of a source-card wall.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — A visitor sees the product and the vision (Priority: P1)

A visitor walks up to a booth screen. The first poster shows a browser/monitor with a real DOOM 3
frame, completely redacted in `XX`, a live HUD, and four capability cards: PLAY, REMAP, MOD and
AI ROGUE. Chinese voice-over explains the vision; captions carry both languages.

**Why this priority**: The deck is a marketing artifact. The first poster must sell "DOOM 3 in a
web page" and the platform vision within seconds.

**Independent Test**: Open `showcase/index.html`; the monitor contains `hero-redacted.png`, the
`REDACTED CAPTURE` badge is visible, four capability cards exist, and the AI Rogue card is marked
`CONCEPT`.

**Acceptance Scenarios**:

1. **Given** the file is opened offline, **When** poster 1 appears, **Then** the monitor shows a
   redacted real frame with a visible `REDACTED CAPTURE / LICENSING PENDING` badge and four
   capability cards.
2. **Given** the narration ends on poster 1, **When** the deck advances, **Then** poster 2 sells the
   modern-FPS/AI-native dream and poster 3 shows the platform vision.
3. **Given** the deck loops from poster 3, **Then** it returns to poster 1 without reloading.

---

### User Story 2 — A presenter controls the deck and demos the AI Rogue idea (Priority: P2)

A presenter pauses, steps forward/back, mutes, toggles captions, and clicks **GENERATE RUN** on
poster 3. The prompt, seed, map and modifier change; the panel stays labelled as a concept.

**Independent Test**: Press ArrowRight/Left, Space, M, C. On poster 3, click `GENERATE RUN`; the
prompt text changes and the output contains a new seed. No console errors.

**Acceptance Scenarios**:

1. **Given** poster 1, **When** ArrowRight is pressed, **Then** poster 2 is active within 300 ms.
2. **Given** the AI Rogue card, **When** `GENERATE RUN` is clicked, **Then** the prompt and seed
   change and the button disables during generation.
3. **Given** narration is playing, **When** Space is pressed, **Then** audio pauses and resumes from
   the same offset.

---

### User Story 3 — A maintainer replaces the redacted frame with a licensed capture (Priority: P3)

A maintainer supplies a licensed image or video, rebuilds, and the monitor uses it with the correct
label and credit. If no licensed asset exists, `--no-hero` restores the procedural reference frame.

**Independent Test**: `uv run --python 3.12 showcase/build.py --hero <licensed.png> --hero-label
"LICENSED CAPTURE" --hero-credit "..." --out <temp.html>`; the temp build shows the media and the
credit. `--no-hero` produces the procedural version. No original game data enters git.

**Acceptance Scenarios**:

1. **Given** the default asset, **When** the build runs with no arguments, **Then** the redacted
   capture is inlined and the label reads `REDACTED CAPTURE`.
2. **Given** `--no-hero`, **When** the build runs, **Then** the procedural industrial frame is used
   and the monitor badge says `PROCEDURAL · NOT A GAME CAPTURE`.
3. **Given** a licensed capture, **When** built with `--hero` and `--hero-credit`, **Then** the
   credit is visible and the file is not committed.

### Edge Cases

- Autoplay blocked: silent wall-clock rotation with captions, plus a one-click sound gate.
- Reduced motion: canvas animation stops, audio/captions still advance.
- Offline: every request is `file:`, `data:` or `blob:`.
- Redacted asset missing: `--no-hero`/fallback uses the procedural frame; no blank monitor.
- AI button clicked rapidly: disabled for 680 ms; no overlapping timers.
- Touch: hero media has `pointer-events:none` and `draggable=false`, so a swipe is not swallowed.
- 4K and portrait: the 1920×1080 stage scales; the monitor remains legible at 4K and letterboxes on
  portrait.
- Claim drift: verifier asserts the six roadmap literals.
- Licensing: the original screenshot is not in the repo; the redacted asset is marked pending.

## Requirements *(mandatory)*

- **FR-001**: The deliverable MUST be one self-contained `showcase/index.html` that works from
  `file://` with no network.
- **FR-002**: It MUST contain exactly three posters: PLAY, THE DREAM, VISION.
- **FR-003**: Each poster MUST have embedded Chinese voice-over and word-synced bilingual captions.
- **FR-004**: Users MUST be able to navigate, pause/resume, mute, toggle captions and fullscreen with
  mouse, keyboard and **real touch** input. Touch MUST be verified with CDP touch events, not mouse
  pointer events. Space on a focused control MUST activate that control, not toggle playback.
- **FR-005**: Autoplay blockage MUST fall back to a silent timed rotation and a one-click sound
  recovery without a reload.
- **FR-006**: A fixed 1920×1080 stage MUST scale to any viewport without reflow or overflow.
- **FR-007**: The six quantitative evidence claims MUST live in README/ROADMAP with a `ROADMAP §1`
  source, and poster 2 MUST carry a one-line proof strip (1,387 / 734,180 / 0 game data). Poster 2
  MUST NOT be an evidence-card wall. HUD values and generated timecodes are illustrative.
- **FR-008**: The deck MUST label the build `M0 · SCAFFOLD` (with “port work starts at M1”), the
  vision roadmap `CONCEPT ROADMAP / 2026`, and the AI Rogue demo `CONCEPT DEMO · NOT A SHIPPED
  FEATURE`. The lead MUST be future tense (`will run`, `将在里面跑`), and M0 MUST NOT claim a CI baseline that is
  absent from the repository. It MUST NOT claim a shipped port or shipped AI feature.
- **FR-009**: Poster 1 MUST render a browser/monitor mockup with a real frame. The committed default
  MUST be the redacted `assets/hero-redacted.png`; `--no-hero` MUST restore the procedural frame.
- **FR-010**: Poster 1 MUST show four capability cards (PLAY, REMAP, MOD, AI ROGUE) with status tags
  (`CORE`, `M5`, `M7`, `CONCEPT`).
- **FR-011**: The AI Rogue card MUST provide an interactive concept demo: `GENERATE RUN` cycles the
  prompt, seed, map and modifier; the panel MUST remain labelled as a concept.
- **FR-012**: The redacted asset MUST be labelled `REDACTED CAPTURE` and credited `LICENSING
  PENDING · NOT A LICENSED RELEASE`; the original screenshot MUST NOT be committed. `--hero`,
  `--hero-label`, `--hero-credit` and `--out` MUST allow a licensed replacement.
- **FR-013**: The build MUST be reproducible from `narration.json` and the authored source parts,
  MUST reuse a local TTS cache when available, and two consecutive default builds MUST be byte-identical.
- **FR-014**: The deck MUST expose `?page=N`, `?snap=1`, `?mute=1`, `?autoplay=0`, and `?cc=0`.
- **FR-015**: `showcase/start.cmd` MUST launch Edge kiosk with the autoplay exemption and a local
  profile, without tracked machine-specific paths.
- **FR-016**: `showcase/verify.py` MUST mechanize the contract: offline audit, three posters, zero
  console errors, narration, captions, claims, redacted-hero label, capability cards, AI Rogue,
  controls, autoplay fallback, screenshots.
- **FR-017**: `showcase/verify_deep.py` MUST cover Edge + Chrome, offline, reduced motion, **real CDP
  touch**, Space-on-control, page visibility, canvas-unavailable fallback, loop, caption sync,
  evidence hovers, mute, fullscreen, 4K + DPR2, portrait, animation, performance, accessible names,
  HTML parse and `start.cmd`.
- **FR-018**: The framework MUST degrade to static visuals when animation is unavailable or reduced
  motion is requested, without losing DOM content, captions or navigation. A missing 2D canvas
  context MUST NOT prevent boot, captions or paging.
- **FR-019**: Poster 2 MUST sell the modern-FPS/AI-native experience: a 2026 vision render (procedural,
  labelled `NOT A GAME CAPTURE`), a 2004 archival diptych, and three cards (NEXT-GEN LOOK, AI DIRECTOR,
  PERSONAL RUN). It MUST NOT be a six-card source/evidence wall.

## Key Entities

- **RedactedCapture**: The 1920×1080 `XX`/mosaic asset and its provenance note; licensing pending.
- **CapabilityCard**: PLAY / REMAP / MOD / AI ROGUE, each with a status tag.
- **RogueDemo**: The prompt console with prompt, seed, map and modifier; concept only.
- **EvidenceClaim**: A number plus `ROADMAP §1` and an evidence command.
- **LoopStep**: ISSUE → WORKTREE → SPEC → BUILD → VERIFY → REVIEW → MERGE.

## Success Criteria *(mandatory)*

- **SC-001**: `index.html` is one file between 100 KB and 4 MB with no external subresource.
- **SC-002**: Zero console errors and zero page errors on all three posters.
- **SC-003**: Three 1920×1080 screenshots, non-blank and pairwise different.
- **SC-004**: Narration total 50–85 s; each poster at least 12 s; at least five caption chunks per
  poster (actual 6/7/7, verified by seeking every chunk).
- **SC-005**: ArrowRight changes poster within 300 ms; Space toggles pause; V is not required.
- **SC-006**: The six roadmap literals are present in README with `ROADMAP §1`, and the poster proof
  strip carries 1,387 / 734,180 / 0 game data.
- **SC-007**: A full `file://` load with external requests blocked produces no external request.
- **SC-008**: `?page=N&snap=1` is visually settled; all active reveals are opacity ≥ 0.99.
- **SC-009**: Poster 1 shows the redacted capture, the `REDACTED CAPTURE` label and the four
  capability cards; `--no-hero` produces the procedural frame.
- **SC-010**: `GENERATE RUN` changes the prompt, seed, map and modifier and keeps the `CONCEPT DEMO` note.
- **SC-011**: axe-core WCAG 2.1 A/AA reports zero violations on all three posters after settling;
  the 1 incomplete color-contrast check per poster (text over image/canvas) MUST be recorded rather
  than presented as full AA verification.
- **SC-012**: Poster 2 contains the 2026 vision render, the 2004 archival frame, the three experience
  cards and the proof strip; it contains no six-card source wall.

## Assumptions

- The repository owner accepts the redacted derivative as a placeholder and will replace it before
  public distribution; the original screenshot is not committed.
- Firefox is not installed on this machine; the deep matrix records one skip.
- The 77.4 s narration is the accepted length; the old 64 s script is replaced.
- The AI Rogue demo is deterministic and local; it does not call a model or network.
- The redacted image is committed at `showcase/assets/hero-redacted.png` (≈0.71 MiB); the deck total
  is ≈1.61 MiB.
