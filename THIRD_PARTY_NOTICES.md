# Third-party notices

This repository's own content (specs, docs, scripts, workflow, automation) is licensed under
**GPL-3.0-or-later**; the full text is in [`LICENSE`](LICENSE). The third-party works below are
included or referenced under their own licences, which this repository does not override.

## DOOM 3 GPL Source Code — git submodule

- Path: `doom3_orig/doom3/` (a submodule, not vendored files)
- Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company
- Licence: GNU General Public License, version 3 or later — `doom3_orig/doom3/COPYING.txt` and the
  per-file headers
- Upstream: <https://github.com/id-Software/DOOM-3> (fork: <https://github.com/yhyu13/DOOM-3>)
- The source release contains **no game data**. DOOM 3 assets remain under the original EULA and
  are neither GPL-licensed nor distributed by this repository.

## GitHub Spec Kit

- Paths: `.specify/`, `.kilo/command/speckit.*.md`
- Licence: MIT
- Source: <https://github.com/github/spec-kit> at commit `67e0f72` (templates, scripts, and command
  prompts installed by `specify init`)

## Agent skills

- Paths: `.kilo/skills/<name>/`
- Licences:
  - `cross-platform-guide` — MIT, `xonovex/platform` (pinned in [`docs/SKILLS.md`](docs/SKILLS.md))
  - `wasm-emscripten` — MIT, `mohitmishra786/low-level-dev-skills` (pinned there)
  - `webapp-testing` — Apache-2.0 (`LICENSE` and `LICENSE.txt` in its directory)
  - the remaining skills are copies from the operator's AgentSkillsVault; per-skill provenance is
    recorded in [`docs/SKILLS.md`](docs/SKILLS.md)
- MIT and Apache-2.0 are compatible with GPLv3. Each skill keeps its own notice and must not be
  relicensed by edits to this file.

## MIT licence text (applies to the MIT-licensed items above)

```
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
