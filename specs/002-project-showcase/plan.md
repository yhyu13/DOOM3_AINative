# Implementation Plan: DOOM 3 Browser Product Concept Deck

**Branch**: `feat/2-project-showcase` | **Date**: 2026-10-26 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/002-project-showcase/spec.md`

## Summary

Build one offline, self-contained HTML product concept with three posters:

1. **PLAY** — a browser/monitor mockup with a **redacted real DOOM 3 frame**, live HUD and four
   capability cards (PLAY, REMAP, MOD, AI ROGUE).
2. **THE DREAM** — a 2004 / 2026 diagonal comparison: redacted 2004 archival on the left, a procedural
   2026 modern corridor on the right, a draggable diagonal, REVEAL 2026 toggle and a 2.6 s idle auto-sweep with a damped spring, and NEXT-GEN LOOK /
   AI DIRECTOR / PERSONAL RUN. The six evidence numbers move to README and a one-line proof strip;
   the poster sells the imagination, not the difficulty.
3. **VISION** — REMAP/MOD/AI ROGUE concept cards, an interactive one-sentence Rogue demo, the
   seven-step delivery loop and the M0–M7 roadmap.

A Python build script turns `narration.json` into Chinese Edge TTS audio, captures word boundaries,
inlines the redacted hero and assembles `index.html`. A contract verifier and a deep matrix prove
the result in Edge and Chrome.

## Technical Context

**Language/Version**: HTML5, CSS, ES2020 (browser); Python 3.12 via `uv` (build and verify)

**Primary Dependencies**: `edge-tts==7.3.1`, `mutagen` (build); `playwright`, `pillow`, `html5lib`
(verify). No browser framework, no bundler, no CDN, no Three.js.

**Storage**: None. Audio, timings, the redacted image and page data are embedded in the HTML. A
git-ignored TTS cache lives in `showcase/evidence/tts-cache.json`.

**Testing**: `showcase/verify.py` (contract, Edge) and `showcase/verify_deep.py` (matrix, Edge +
Chrome), plus an ad-hoc axe-core WCAG 2.1 A/AA audit.

**Target Platform**: Desktop browser, primary Microsoft Edge on Windows; `file://`; 1920×1080 stage
scaled to viewport.

**Project Type**: Static single-file presentation artifact plus reproducible build/verification
scripts.

**Performance Goals**: Target 60 fps canvas; measured 56.3–60.3 fps across runs on this machine (gate ≥30); first render under 1.5 s; file under
4 MB; no layout shift (system fonts only).

**Constraints**: Fully offline at presentation time; original screenshot not committed; redacted
asset labelled licensing-pending; no game data; no absolute machine paths; autoplay fallback;
keyboard controls; reduced motion; touch.

**Scale/Scope**: Three posters, 82.8 s narration, four capability cards, three dream cards, three vision cards, seven
loop steps, eight roadmap milestones, six numeric claims.

## Constitution Check

| Gate | Status | Evidence |
|---|---|---|
| Pristine submodule untouched | PASS | Scope is `showcase/**` and `specs/002-project-showcase/**`. |
| No game data committed | PASS | The original screenshot is not in git; the redacted placeholder is tracked and labelled. No `.pk4/.pk3/.pak`. |
| No direct commit to `main` | PASS | Worktree `feat/2-project-showcase`; PR required. |
| Evidence before victory | PASS | 123 contract checks, 161 deep checks, axe 0 violations (1 incomplete/poster); logs/screenshots. |
| No secrets or absolute machine paths | PASS | `start.cmd` uses `%~dp0` and `%ProgramFiles%`; README uses repo-relative sample paths. |
| One writer per file | PASS | Single worker owns the scope. |
| Spec before plan before tasks | PASS | `spec.md`, this file, `tasks.md`. |

## Project Structure

```text
specs/002-project-showcase/
├── spec.md
├── plan.md
└── tasks.md

showcase/
├── index.html                 # generated single-file deliverable
├── assets/
│   ├── hero-redacted.png      # redacted real frame, licensing pending
│   └── README.md              # provenance + redaction method
├── src/
│   ├── head.html
│   ├── base.css
│   ├── posters.css
│   ├── body.html
│   ├── runtime.frame.js       # procedural industrial fallback
│   ├── runtime.compare.js     # 2004 / 2026 modern comparison render
│   ├── runtime.canvas.js      # portal / rain / graph
│   ├── runtime.core.js        # audio, captions, controls, AI Rogue demo
│   └── narration.json
├── build.py
├── verify.py
├── verify_deep.py
├── start.cmd
├── preview.png
└── README.md
```

## Design Decisions

1. **Single generated artifact.** Authored head/CSS/body/JS and `narration.json` are assembled into
   one `index.html` with embedded MP3s, timings and the redacted image. Never hand-edit the artifact.
2. **Redacted real frame as the default.** The owner asked for a real DOOM 3 frame in a browser. The
   official promo screenshot is transformed into an `XX`/mosaic/`REDACTED` asset. The original is not
   committed. `--no-hero` restores the original procedural corridor; `--hero` accepts a licensed
   image/video with a label and credit.
3. **Browser-monitor mockup.** A browser chrome, URL bar, live badge and monitor bezel frame the
   game view; a DOM HUD (health/armor/ammo, sector, WASD hint) sells the in-page game concept.
   The game view is `hero-redacted.png` inside `.browser-view`; the portal canvas remains behind it.
4. **Capability cards with honest tags.** PLAY (`CORE`), REMAP (`M5`), MOD (`M7`), AI ROGUE
   (`CONCEPT`). The roadmap and status plate keep the deck from claiming a shipped port.
5. **AI Rogue concept demo.** A local, deterministic prompt console cycles four runs (prompt, seed,
   map, modifier), disables the button during generation and keeps the
   `CONCEPT DEMO · NOT A SHIPPED FEATURE` note.
6. **Audio-first with a silent clock.** The runtime tries `play()`, falls back to a wall-clock
   rotation with captions, and restores sound on one click.
7. **Deterministic screenshots.** `?snap=1` disables reveal transitions, freezes canvases and settles
   the caption. `?page=N` selects a poster.
8. **TTS cache.** `showcase/evidence/tts-cache.json` makes rebuilds fast and lets the hero tests run
   without network after the first synthesis.
9. **Kiosk launcher.** `start.cmd` starts Edge with `--kiosk`,
   `--autoplay-policy=no-user-gesture-required` and a local profile.
10. **Two verification layers.** `verify.py` is the PR contract (123 checks); `verify_deep.py` covers
    the matrix (161 checks: Chrome, offline, reduced motion, **real CDP touch**, Space-on-control,
    visibility, canvas fallback, sync, 4K + DPR2). `axe-core` is run separately for WCAG AA; the
    color-contrast incomplete nodes over image/canvas are recorded.
11. **Sell the dream, prove it elsewhere.** Poster 2 is the 2004/2026 comparison + AI-native
    experience concept; the six roadmap numbers live in README and a one-line proof strip. This keeps the
    evidence traceable without making players read a code audit.

## Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Redacted derivative still carries rights | Original not in git; asset labelled `LICENSING PENDING`; replace with licensed capture; provenance note. |
| Autoplay blocked at a booth | Kiosk flag; silent-clock fallback; click gate. |
| Real frame provokes "is this shipped?" | `M0 · SCAFFOLD`, `CONCEPT` tags, redaction bar. |
| AI Rogue over-promises | `CONCEPT DEMO · NOT A SHIPPED FEATURE`; deterministic local demo only. |
| Hero image / stage swallows touch | `pointer-events:none`, `draggable=false`, `#stage{touch-action:none}`; real CDP touch test asserts no `pointercancel`. |
| Poster 2 becomes a source lecture | The dream page is visual; the six evidence numbers move to README + a one-line proof strip. |
| Claim drift / overstated copy | Verifier asserts six literals; `M0 · SCAFFOLD`, future-tense lead and `CONCEPT` labels. |
| Byte drift | Two consecutive default builds hashed byte-identical. |
| Large file | 1.61 MiB total; 4 MiB budget; verifier checks size. |
| Firefox unavailable | Deep matrix records one skip; Edge/Chrome both pass. |

## Phases

- **Phase 0 — grounding**: roadmap facts, workflow, TTS/Playwright probes, licence audit.
- **Phase 1 — design**: product concept, browser monitor, capability cards, AI Rogue demo, copy.
- **Phase 2 — build**: redacted asset, template parts, TTS, assembly, cache.
- **Phase 3 — verify**: contract, deep matrix, axe, human visual pass, fixes.
- **Phase 4 — ship**: README/spec/tasks, commit, push, PR, independent review.
