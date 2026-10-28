# DOOM3_AINative — three-poster product concept

**One offline HTML file. Three posters. A browser-monitor product hero, a real (redacted) DOOM 3 frame, and a one-sentence Rogue-mode concept.**

Open [`index.html`](index.html) directly, or double-click [`start.cmd`](start.cmd) for a full-screen
Microsoft Edge kiosk with the autoplay exemption.

![Three-poster preview](preview.png)

## What the deck says

| Poster | Claim | What it shows |
|---|---|---|
| **01 · PLAY** | *DOOM 3 in the browser.* | A monitor/browser mockup with a **real DOOM 3 frame, fully redacted in `XX`**, a live HUD, and four capability cards: **PLAY / REMAP / MOD / AI ROGUE**. |
| **02 · THE DREAM** | *Modern FPS. Personal hell.* | A **2004 / 2026 diagonal comparison**: the left side is the **redacted 2004 archival frame**, the right side a **GPT Image 2.5 modern-FPS vision** (image-to-image from the 2004 promo; PBR, volumetric light, HDR). Drag the knob, press **REVEAL 2026**, or let it auto-sweep after 2.6 s idle; **REVEAL 2026** expands the modern side (32), **BACK TO 2004** expands the old side (80); **NEXT-GEN LOOK / AI DIRECTOR / PERSONAL RUN**; one proof strip. |
| **03 · VISION** | *Your DOOM. Your rules.* | Over a **GPT Image 2.5 Mars command-center vision**: **REMAP** (M5 planned), **MOD** (M7 planned), and **AI ROGUE** — a working concept demo where one sentence regenerates a seed, a map and a modifier. Plus the seven-step delivery loop and the M0–M7 roadmap. |

The deck is a **product concept**, not a claim that the port is shipped. Poster 1 uses future-tense
product copy (`will run`), carries `M0 · SCAFFOLD`, and says port work starts at M1; poster 3
labels the roadmap `CONCEPT ROADMAP / 2026`, and the AI Rogue panel says
`CONCEPT DEMO · NOT A SHIPPED FEATURE`.

## The redacted capture

The real frame in the monitor is a **heavily redacted** derivative of an official DOOM 3
promotional screenshot from the Steam store page (app 9050, © id Software / Bethesda / ZeniMax).
No reuse licence is granted. The original screenshot is **not** in this repository.

The committed `assets/hero-redacted.png` is:

- cropped to 16:9, downscaled to 48×27 and re-expanded with nearest-neighbour (deliberate mosaic);
- ember-graded, darkened and vignetted;
- covered by a full-frame **56×30 `XX` grid**;
- stamped `REDACTED` / `LICENSING PENDING · NOT A LICENSED RELEASE`;
- finished with scanlines.

It preserves the shape of a real frame while removing readable art. It is a **placeholder**, not a
licensed release. See [`assets/README.md`](assets/README.md). Replace it with a licensed capture
before public distribution:

```powershell
uv run --python 3.12 showcase/build.py `
  --hero "..\captures\licensed-frame.png" `
  --hero-label "LICENSED CAPTURE" `
  --hero-credit "Capture: your name / used with permission"
```

A local, unredacted design preview was used only in the operator's temp directory and is not part of
this repository. The rejected alternatives (Doom Wiki fair use, CC BY 2.0 QuakeCon photo, CC BY-SA
2.5 fan art) are not GPL-compatible; the licence audit is recorded in the project conversation.

## AIGC vision (GPT Image 2.5)

The two vision images are generated with **GPT Image 2.5** (`openai/gpt-image-2.5-flare`) through an
OpenAI-compatible proxy:

| Asset | Endpoint | Input | Role |
|---|---|---|---|
| `assets/aigc/page2-modern.jpg` | `POST /v1/images/edits` | official 2004 promo frame (local only) | 2026 modern-FPS reimagining; right side of poster 2 |
| `assets/aigc/page3-vision.jpg` | `POST /v1/images/generations` | text only | Mars command center / holographic level map; poster 3 backdrop |

Both are cropped to 16:9, resized to **1600×900 JPEG q86**, and embedded as data URIs. The deck
labels them **`AI-GENERATED VISION · NOT A GAME CAPTURE`**; page 3 also carries
**`AI VISION · GPT IMAGE 2.5`**. The prompts, endpoints and licensing status are in
[`assets/aigc/README.md`](assets/aigc/README.md).

- Page 2 is a **derivative** of the official promo screenshot. The original and the unredacted input
  are **not** in this repository; licensing is **pending** (T042 / issue #2). The owner accepted
  this risk when the showcase was merged: it is a concept vision, **not** a licensed capture.
  Replace it if a cleared distribution is required.
- Page 3 is **text-to-image** with no game-art input; it is original AI-generated concept art.
- `--no-aigc` restores the procedural vision fallback. It does **not** remove the redacted hero:
  poster 1 and the page-2 archival inset still use it, so the PNG is stored once. It is not a
  licensing-clean build.

## Controls

| Input | Action |
|---|---|
| `←` / `→` or swipe | Previous / next poster |
| `Space` | Pause / resume |
| `M` | Mute / unmute |
| `C` | Toggle captions |
| `F` | Fullscreen |
| `GENERATE RUN` (poster 3) | Regenerate the AI Rogue concept prompt, seed, map and modifier |
| Bottom bar | Prev, play/pause, next, sound, captions, fullscreen |

The deck auto-advances when a poster's narration ends and loops from poster 3 to poster 1. If the
browser blocks autoplay, it falls back to a silent wall-clock rotation with captions and shows a
**CLICK FOR SOUND** button; one click restores audio from the current poster without reloading.

### Query parameters

| Parameter | Effect |
|---|---|
| `?page=1\|2\|3` | Start on a specific poster |
| `?snap=1` | Settle all entrance transitions and freeze the canvas — used for screenshots |
| `?mute=1` | Start muted (captions still advance) |
| `?autoplay=0` | Start paused |
| `?cc=0` | Start with captions hidden |

## Kiosk launch

```powershell
.\showcase\start.cmd
```

`start.cmd` resolves its own directory, starts Edge with `--kiosk`, sets
`--autoplay-policy=no-user-gesture-required`, and keeps a local `edge-profile/` (git-ignored). Exit
kiosk with `Alt+F4`. Do not point it at a network URL: the deck is designed to run from `file://`.

## Narration

Voice: `zh-CN-YunxiNeural`, rate `+8%`. Total: about **82.8 seconds**.

**01 · PLAY**

> 想象一下，打开一个网页，DOOM 3 将在里面跑。不用安装，不用云游戏，不用模拟器。WebAssembly 和 WebGL2，将把二〇〇四年的 idTech4 引擎，直接搬进浏览器。鼠标锁定，手柄，改键，Mod，全部在网页里。这，就是 DOOM3_AINative。我们正在把它变成现实。

**02 · THE DREAM**

> 忘掉源码有多难。左边是 2004 年的存档画面，已经脱敏；右边是 2026 年的现代渲染。拖动这个圆点，看同一座火星基地能变成什么样。PBR、HDR、体积光、4K、120 帧。然后，AI 接管导演。它读懂你的玩法，重写节奏、恐惧和敌人。每一局，都是为你生成的。同一座基地，不同的地狱。这不是重制，这是重生。

**03 · VISION**

> 改键，Mod，AI 一句话生成 Rogue 模式。一句话，生成地图、规则和敌人。这不是一个启动器，而是一个 AI 原生的游戏平台。一个议题，一个分支，一个工作树，一个 PR。规格、实现、独立审查、证据门禁。从 M0 到 M7，每一步都由证据推进。目标，是让 DOOM 3 在浏览器里重生。

The English translation is the second caption line. Both live in
[`src/narration.json`](src/narration.json).

## Rebuild

The committed `index.html` already contains the audio, the redacted capture and the two AIGC
vision images. Rebuilding is only needed when the script, voice, assets, or licensed media changes.

```powershell
uv run --python 3.12 showcase/build.py
uv run --python 3.12 showcase/build.py --no-aigc   # procedural corridor/network fallback
```

By default the build uses `assets/hero-redacted.png` and the two `assets/aigc/*.jpg` images. Use
`--no-hero` for the procedural industrial reference frame on poster 1 (page 2 keeps the redacted
archival inset), `--no-aigc` for the procedural corridor/network fallback, or `--hero` for a
licensed image/video. The first build needs network access to Microsoft Edge TTS; later builds
reuse the git-ignored cache in `showcase/evidence/tts-cache.json`.

```text
showcase/
├── index.html                 generated single-file deliverable
├── assets/
│   ├── hero-redacted.png      redacted real frame (placeholder, licensing pending)
│   ├── README.md              provenance + redaction method
│   └── aigc/
│       ├── page2-modern.jpg   GPT Image 2.5 image-to-image vision (licensing pending)
│       ├── page3-vision.jpg   GPT Image 2.5 text-to-image command center
│       └── README.md          model, endpoints, prompts and licensing status
├── src/
│   ├── narration.json         the only transcript source
│   ├── head.html              document head + meta
│   ├── base.css               tokens, chrome, captions, controls
│   ├── posters.css            poster 1/2/3 layouts
│   ├── body.html              DOM
│   ├── runtime.frame.js       procedural industrial reference frame (fallback)
│   ├── runtime.compare.js     2004 / 2026 modern comparison render
│   ├── runtime.canvas.js      portal / code rain / workflow graph
│   └── runtime.core.js        audio, clock, captions, controls, AI Rogue demo
├── build.py                   TTS + timings + assembly + hero media
├── verify.py                  Playwright contract gate
├── verify_deep.py             extended matrix (Chrome/Edge, offline, touch, sync)
├── verify_variants.py         default / --no-aigc / --no-hero / --hero / reproducibility gate
├── verify_axe.py              axe-core WCAG 2.1 A/AA gate (axe-core not vendored)
├── start.cmd                  Edge kiosk launcher
└── preview.png                3-up preview for this README
```

**Do not hand-edit `index.html`.** Change `src/` or the asset and rebuild.

## Verification

```powershell
uv run --python 3.12 showcase/verify.py
uv run --python 3.12 showcase/verify_deep.py
uv run --python 3.12 showcase/verify_variants.py
# axe-core is not vendored; install it once, then point AXE_PATH at it
npm install axe-core@4.10.3 --prefix .axe
$env:AXE_PATH = ".axe/node_modules/axe-core/axe.min.js"
uv run --python 3.12 showcase/verify_axe.py
```

Current evidence (2026-10-28):

- `verify.py` — **171 passed, 0 failed**. Single file 2,741,798 bytes (2.61 MiB), zero external
  requests, three posters, zero console/page errors, three distinct screenshots, embedded narration,
  six claim literals, redacted-hero label, four capability cards, AI Rogue concept,
  keyboard/pause/captions, autoplay-blocked fallback, `?autoplay=0`, 1366 scaling. It also asserts
  the exact data URI and decoded 1600×900 size of each AIGC image, that each image is **actually
  visible** (computed style/rect, not only `!hidden`), that the rendered crop **correlates with the
  asset** (negative control: a hidden image must correlate <0.3), the exact truth labels on pages 2
  and 3, the single embedded redacted PNG, the visual split/clip (not just `aria-valuenow`), real
  pointer drag, and both sides of the auto-sweep (72 → 40).
- `verify_deep.py` — **186 passed, 0 failed, 1 skipped**. Edge + Chrome, offline `file://`, reduced
  motion, `cc=0`, **real CDP touch swipe (no `pointercancel`)**, 3→1 loop, caption + word sync at all
  21 chunks, Space-on-control, page visibility, canvas-unavailable fallback, evidence hovers, mute,
  fullscreen, **4K posters 1/2/3**, DPR2, portrait, animation, **60.4–60.5 fps measured in this run**
  (gate ≥30), accessible button names, HTML parse, and the `start.cmd` contract. The one skip is
  Firefox: not installed.
- **axe-core 4.10.3, WCAG 2.1 A/AA** — **0 violations** on all three posters after a 900 ms settle;
  **1 incomplete color-contrast check per poster** (120/66/113 nodes, text over image/canvas), so AA
  contrast is not fully machine-verified. Rerun with `verify_axe.py`; the JSON lands in
  `showcase/evidence/axe.json`.
- **Variant gate** — `verify_variants.py` rebuilds the default twice plus `--no-aigc`, `--no-hero`
  and a licensed `--hero` deck and checks exact assets/labels, the 46→80→32 visual clip, pointer
  drag, static and runtime fallback labels, procedural canvases, licensed-hero isolation, local-only
  requests and reproducibility. Last run: **63 passed, 0 failed**.
- **Reproducibility** — two consecutive default builds are byte-identical and match the committed
  artifact: 2,741,798 bytes, SHA-256
  `b758b90a9cd2bc1a98af0b6cae853a9f21348156d2a2281e30032f043e864afc`.

Screenshots and logs land in `showcase/evidence/`, which is git-ignored. `verify.py --headed` lets
you watch the run.

## Proof, moved off the poster (ROADMAP §1)

Players do not need the source wall. The six measured claims live here and in
[`docs/ROADMAP.md`](../docs/ROADMAP.md) §1, captured 2026-10-26 from the pristine submodule at
`a9c49da5`:

- 1,387 C/C++ files; 734,180 lines of code
- 100 fixed-function calls; 207 ARB-program references
- 0 pak archives; 0 Emscripten references

The workflow wording comes from [`docs/GIT_WORKFLOW.md`](../docs/GIT_WORKFLOW.md) and
[`AGENTS.md`](../AGENTS.md). If the roadmap numbers change, update this section and run the
verifier: it asserts the README literals and the poster proof strip.

## Licensing

- This repository's content is **GPL-3.0-or-later**.
- DOOM 3 engine source is GPLv3; game data is **not** included and remains under the original EULA.
- DOOM 3 is a trademark of id Software / ZeniMax. This project is unaffiliated.
- `assets/hero-redacted.png` is a transformed derivative of an official promotional screenshot.
  Rights remain with id/Bethesda; it is included only as a **redacted placeholder** with
  `LICENSING PENDING`. Replace it with a licensed capture before public distribution. The original
  screenshot is not in this repository.
- `assets/aigc/page2-modern.jpg` is a **GPT Image 2.5 derivative** of the official 2004 promo frame
  (image-to-image). Rights in the underlying art remain with id/Bethesda; it is a concept vision, not
  a game capture, and **licensing is pending** (T042 / issue #2). The owner accepted this risk when
  the showcase was merged; it must not be represented as a licensed capture. The original input is
  not in this repository.
- `assets/aigc/page3-vision.jpg` is **text-to-image** with no game-art input; it is original
  AI-generated concept art for this deck.
- The procedural reference frame, procedural 2026 corridor/network fallback, portal, code rain,
  graph and AI Rogue demo are original code in this repository.
- Narration is generated with Microsoft Edge TTS from the repository's own script.
- Any `--hero` image or video is supplied by the operator; it is inlined into a local build but is
  not part of the repository, and the operator is responsible for its licence.
