# Review — T088 Emscripten encoder leg (issue #35)

## Reviewer independence

Independent agent reviewer, not an author of this branch. Review ran only in
`D:\git-repo-ai\DOOM3_AINative\.kilo\worktrees\feat-43-emscripten-leg`, on branch
`test/35-emscripten-leg` at `cf9764b` (identical to `main`); all change-set files were staged, not
committed. `doom3_orig/doom3` was not touched and is clean at `a9c49da5`. Nothing was committed,
pushed or downloaded; the only tracked file created is this review. The falsification below
temporarily edited `tests/webgpu/emscripten/conformance.cpp`, then restored it byte-for-byte
(SHA-256 `299BBB4ABD2AC23246B79379A68BFB5F753226148ABDA02CF486E6E273CA2B1B`, confirmed before and
after). All other modifications were git-ignored build output.

## Scope and commands

| # | Command | Exit | Observation |
|---|---|---|---|
| 1 | `$env:EMSDK='C:\emsdk'; .\tests\webgpu\emscripten\build.ps1` | 0 | `emcc 6.0.13 (e13ee988…)`; port `emdawnwebgpu`; `index.wasm` 146086 B, `index.js` 63530 B — same sizes as the record |
| 2 | `node tests/webgpu/run-conformance.mjs --page emscripten/index.html --repeat 2 --out build\review-em` | 0 | Chrome 156.0.8078.73; 12 pass / 0 fail / 1 not run on both attempts; `repeatable across 2 attempt(s): true`; `A1-software-adapter` NOT RUN |
| 3 | Mutated `conformance.cpp:487` expected MRT texel `64`→`65`, rebuild | 0 | build still links |
| 4 | Runner against mutated build, `--repeat 1` | **1** | `C4-mrt FAIL … byte 0: 64 != 65`; `required checks not PASS: C4-mrt` |
| 5 | Restore `conformance.cpp`, rebuild, rerun | 0 / 0 | 12 pass / 0 fail / 1 not run; hash matches the pre-mutation file |
| 6 | `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | `57/57 public declarations classified` |
| 7 | `py -3 -I scripts/validate-rhi-contract.py` | 0 | `header scan: RUN over 3 header(s) and 3 public test file(s)` |
| 8 | `git diff --cached --check` | 0 | no whitespace errors |

Additional evidence: `evidence/q7a-emscripten.png` shows the rendered 12/12 table and the orange
64×64 canvas clear, matching the values in the JSON and in reproduction #2.

## Is this real C++/GPU work?

- `index.wasm` imports the port, not stubs: parsing its import section yields
  `env::wgpuInstanceRequestAdapter`, `env::wgpuDeviceCreateBuffer`, `env::wgpuQueueWriteBuffer`,
  `env::wgpuCommandEncoderCopyTextureToBuffer`, `env::emwgpuBufferMapAsync`, etc. (40+ `wgpu*`/
  `emwgpu*` imports). The generated glue contains `navigator.gpu`, 177 `wgpu` and 149 `emwgpu`
  references, so the calls reach the browser WebGPU API.
- The page cannot hardcode results: `shell.html:32-41` only accepts `window.q7aPublish` JSON; the
  JSON is built in C++ (`conformance.cpp:826-863`) from check outcomes. The nonce is read inside
  C++ (`conformance.cpp:872-877`) and the runner enforces it for both pages at
  `run-conformance.mjs:202-205`; `--page`/`--root` (`run-conformance.mjs:43,57-58,178,232`) do not
  bypass it.
- Falsification #3–#5 shows the reported status depends on the compiled expectation, not on the
  shell page.

## `wasm32` and the T089 rule

- `conformance.cpp:868` compiles `static_assert(sizeof(void*) == 4, …)` and the build succeeds, so
  the translation unit (including `#include "rhi/rhi.h"` at line 22 via `-I src\renderer`) is a
  32-bit wasm build.
- `doom3::rhi::AdapterFact`/`reportedFacts` are used (`conformance.cpp:915-921`): with
  `vendorId=0`/`deviceID=0` the mask stays `0x0`, and the detail string records that
  (`conformance.cpp:929-936`, reproduced as `reportedFacts=0x0`). The rule is therefore *exercised*
  (computed from the real adapter) but not *asserted* — see m4.

## Findings

### BLOCKING
None.

### MAJOR

- **M1 — dependency section contradicts the completed tasks.** `specs/004-quality-renderer/tasks.md:200`
  still says the Q7a gate is one "of which the lexical and structural gates are done and the encoder
  leg and T089 remain", while `tasks.md:185` and `tasks.md:188` mark T088 and T089 `[x]`, and
  `traceability.md:27` / `plan.md:84` record both complete. This makes the "Q2 unblocked"
  statement internally inconsistent in the same file. Update the dependency paragraph.
- **M2 — the header-scan count in the evidence is wrong.** `evidence-q7a.md:50` and `plan.md:84`
  say the lexical gate scans **1 header**; the executed validator reports **3 headers**
  (`src/renderer/rhi/rhi.h`, `src/renderer/shader/shader_json.h`,
  `src/renderer/shader/shader_resolver.h`). The result ("zero hits") reproduces, but the recorded
  table is not reproducible as written, and it hides that the gate now also covers the shader
  headers — which is precisely why the `shader_resolver.h` reword was needed. Correct both to 3.

### MINOR

- **m1 — unverifiable commit identity.** `evidence-q7a.md:19-20` calls the branch "the commit that
  carries this record", but `HEAD == main == cf9764b` and every change is staged only; no commit
  exists yet. Phrase it as pending, or record the SHA after committing.
- **m2 — changed-surface list overstates.** `evidence-q7a.md:24-26` lists `webgpu-mapping.md` as
  part of this change set; it is not in this branch's diff (landed earlier).
- **m3 — staged evidence differs from the file on disk.** `specs/004-quality-renderer/evidence/q7a-emscripten.json`
  reports `AM`: the index and worktree differ only in `page.runNonce`
  (`q7a-1793269656914-1` staged vs `q7a-1793269767047-1` on disk). Re-stage the intended run.
- **m4 — the T089 rule is observed, not gated.** `conformance.cpp:912-921` derives
  `reportedFacts`, but `C1-device` (`conformance.cpp:929-936`) always records PASS once a device
  exists, regardless of the ids. `README.md:40-42` and `evidence-q7a.md:91-93` are accurate that it
  was exercised with `0x0`, but a future adapter that exposed ids would still pass and silently
  change the claim. Label it "observed, not asserted" (or add an explicit assertion if the point is
  to prove non-fabrication).
- **m5 — authorisation scope of the port download.** `README.md:17-18` authorises installing emsdk;
  the `emdawnwebgpu`/Dawn zip is auto-fetched on first build (`README.md:8,15,17`). T088 says never
  download an unreviewed SDK, so state explicitly that the port download is covered by the same
  owner authorisation (licence BSD-3, version verified).
- **m6 — stale table date.** `AGENTS.md:105` still heads the toolchain table "verified 2026-10-26"
  while the `emcc / emsdk` row documents 2026-10-29.
- **m7 — no elapsed time for the C++ leg.** The runner prints `n/a ms` for this page
  (`run-conformance.mjs:248`) because the C++ JSON omits `elapsedMs`; harmless, but the two legs are
  not timing-comparable.

### Toolchain record

Verified against the host: Emscripten 6.0.13 at `e13ee988ce3d87a738dd8f3661d02223308c412a`,
emsdk checkout `d7b4130`, cached port URL resolving to Dawn `v20260423.175430`
(`.emscripten_url`, `VERSION.txt`), cache path as recorded. Version, commit, licence
(Emscripten MIT/NCSA; Dawn/emdawnwebgpu BSD-3), acquisition and storage are present. No
machine-specific absolute path appears in any tracked change (only `%USERPROFILE%`-relative
discovery in `build.ps1:18-24`). The only authorisation gap is m5.

### Claim check against evidence

Both legs executed (reproduced); software adapter NOT RUN with the reason retained; no WebGPU
implementation of `doom3::rhi::Device` exists (`git ls-files` has no such backend; `rhi.h` contains
only comments); `wasm32` compile proven; no overclaim of browser parity or Q7 support. The one
overclaim is the Q2-unblocked narrative left contradictory by M1.

### Out-of-scope reword

`src/renderer/shader/shader_resolver.h:3` changes "no RHI or D3D12 dependency" to "no RHI or native
graphics API dependency". It is a comment-only, one-line, meaning-preserving (slightly broadening)
change, it removes the only lexical hit that kept `validate-rhi-contract.py` red after the shader
headers landed, and the gate now exits 0 over all three headers. Acceptable.

## Decision

**Changes requested** — the gate itself is sound and reproducible, but M1 (contradictory Q2 gate
statement) and M2 (wrong header count in the signed evidence) must be corrected before merge; m1–m3
should be fixed in the same pass.

I would not sign off the statement that the T089 `reportedFacts` rule is *proven* by this leg: it is
computed and observed as `0x0`, not asserted, so nothing in the pass would fail if a browser started
reporting numeric ids (m4).

---

# Second pass — 2026-10-29 (after author fixes)

Same independence and rules: branch `test/35-emscripten-leg` at `cf9764b` (still equal to `main`; all
changes staged), `doom3_orig/doom3` untouched and clean at `a9c49da5`, no commit, push or download.
Only this review file was edited. The first-pass decision above is superseded by the decision at the
end of this section.

## Re-run

| # | Command | Exit | Observation |
|---|---|---|---|
| 1 | `$env:EMSDK='C:\emsdk'; .\tests\webgpu\emscripten\build.ps1` | 0 | `emcc 6.0.13`; port `emdawnwebgpu`; `index.wasm` **146745 B**, `index.js` 63530 B |
| 2 | `node tests/webgpu/run-conformance.mjs --page emscripten/index.html --repeat 2 --out build\review-em2` | 0 | Chrome 156.0.8078.73; 12 pass / 0 fail / 1 not run twice; `repeatable across 2 attempt(s): true`; C1 detail ends `reportedFacts=0x0 vendorId=0 deviceId=0 facts_consistent=true` |
| 3 | `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | `57/57 public declarations classified` |
| 4 | `py -3 -I scripts/validate-rhi-contract.py` | 0 | `header scan: RUN over 3 header(s) and 3 public test file(s)` |
| 5 | `git diff --cached --check` | 0 | no whitespace errors |

## Resolution of pass-one findings

- **M1 — fixed in `tasks.md`.** `tasks.md:201` now reads "the Q7a header/mapping gate (T088,
  complete)"; the summary at `tasks.md:176-184` is consistent; a whole-file scan found no remaining
  "encoder leg … remain" or "T089 … remain" claim in `tasks.md`.
- **M2 — fixed for the current record.** `tasks.md:176`, `evidence-q7a.md:50` and `plan.md:84` now
  say 3 headers and 3 public test files, matching the executed validator. No current T088 document
  says "1 header" or "52/52"; the older `evidence-q1a.md`, `evidence-t089.md` and `review-q7a.md`
  carry their contemporaneous 1-header/52-52 figures and are historical records, not stale claims.
  The removed `shader_resolver.h` hit is why the count grew.
- **m3 — fixed.** `evidence/q7a-emscripten.json` is now staged as `A` with `facts_consistent=true`
  on both attempts and no unstaged delta.
- **m4 — resolved in wording; assertion is a guard, not proof.** `conformance.cpp:929-944` computes
  `factsConsistent` as "a set fact has a non-zero value" and records C1 with it; the run reports
  `facts_consistent=true`. The wording in `tasks.md:180-182` and `evidence-q7a.md:91-95` now says
  the browser outcome is recorded, not proven, with the positive path left to Q1a — accurate.
  Note the invariant is tautological under the current derivation (`conformance.cpp:915-921` sets a
  bit only when the value is non-zero), so it cannot fail today; it guards against a future
  fabrication, it is not independent evidence of the adapter rule.
- **m1, m2, m5, m6, m7 — still open, non-blocking.** `evidence-q7a.md:20` still calls the branch
  "the commit that carries this record" (no commit exists); `evidence-q7a.md:24` still lists
  `webgpu-mapping.md` in the changed surface; `README.md:17-18` still does not explicitly authorise
  the auto-fetched Dawn port; `AGENTS.md:102` still says "verified 2026-10-26"; the C++ leg still
  reports no `elapsedMs`.

## New findings

### MAJOR

- **M3 — `traceability.md` still contradicts itself about the encoder leg.**
  `specs/004-quality-renderer/traceability.md:3` still says "Q7a T088 is partial … the Emscripten
  encoder leg is NOT RUN, and T089 owns the header reconciliation", and `traceability.md:13`
  (FR-003) still says "Q2 also needs the remaining Q7a encoder leg and T089". The same file's
  FR-017 row at `traceability.md:27` says T088 is **complete** with the Emscripten leg executed and
  T089 complete. This is the same stale-claim class as the fixed M1 and it is in the artifact used
  to justify unblocking Q2. Update lines 3 and 13.

### MINOR

- **m8 — recorded wasm size is stale.** `evidence-q7a.md:67` records `index.wasm` 146086 B, but the
  build that carries the new `facts_consistent` assertion produces 146745 B (and the attached JSON
  is from that new build). Update the recorded size, or say the size is approximate.
- **m9 — README still states the stronger framing.** `tests/webgpu/emscripten/README.md:40-42`
  still says the browser "reports 'not available' instead of inventing ids" without the
  recorded-not-proven caveat added to the evidence; keep it consistent with `evidence-q7a.md:91-95`.

## Final decision

**Changes requested** — the gate itself remains sound: build and run reproduce, the falsification
control holds, the header gate is green over 3 headers, and the T089 wording is now honest. The
blocking item is documentation consistency: `traceability.md:3` and `:13` still declare the
Emscripten leg NOT RUN while `traceability.md:27` declares T088 complete. Fix those two lines (and
preferably m8) and this review turns to accept.

I would not sign off the T088/Q2-gate status until the three `traceability.md` statements agree:
the technical evidence is good, but the shared artifact still tells a reader the required second leg
does not exist.

---

# Third pass — 2026-10-29

Independent agent reviewer, same rules: branch `test/35-emscripten-leg` at `cf9764b` (equal to
`main`; all changes staged), submodule clean at `a9c49da5`, no commit/push/download, only this file
edited.

## Re-run

| Command | Exit | Observation |
|---|---|---|
| `$env:EMSDK='C:\emsdk'; .\tests\webgpu\emscripten\build.ps1` | 0 | `emcc 6.0.13`; `index.wasm` 146745 B, `index.js` 63530 B |
| `node tests/webgpu/run-conformance.mjs --page emscripten/index.html --repeat 2 --out build\review-em3` | 0 | 12 pass / 0 fail / 1 NOT RUN on both attempts; `facts_consistent=true`; repeatable |
| `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | `57/57 public declarations classified` |
| `py -3 -I scripts/validate-rhi-contract.py` | 0 | `header scan: RUN over 3 header(s) and 3 public test file(s)` |
| `git diff --cached --check` | 0 | clean |

## Resolution

- **M3 — fixed.** `traceability.md:3` now says T088 is complete (both legs) and T089 completed the
  reconciliation; `traceability.md:13` (FR-003) no longer says Q2 needs the remaining encoder
  leg/T089; `traceability.md:27` (FR-017) agrees. A full-file search finds no remaining
  "encoder leg NOT RUN" or "T089 open" claim.
- **m8 — fixed.** `evidence-q7a.md:67` records 146745 B with the note that sizes move with the source.

## Remaining items (cosmetic / documentation only)

- `traceability.md:13` repeats "Q2's Q7a/T088 gate is complete" twice in the same cell (cosmetic).
- `tests/webgpu/emscripten/README.md:40-42` still uses the stronger "reports 'not available' instead
  of inventing ids" framing; the evidence now carries the recorded-not-proven caveat (m9).
- Earlier non-blocking minors m1 (`evidence-q7a.md:20` commit identity), m2 (`:24` changed surface),
  m5 (README port-download authorisation), m6 (`AGENTS.md:102` table date) and m7 (no `elapsedMs`)
  remain as records/consistency nits; none affects a claim or a check.

## Final decision

**Accept with limitations.** The required second leg is real, reproducible and sensitive to source
mutation; both validators and the whitespace gate exit 0; the T088/T089/Q2 status is now consistent
in `tasks.md`, `traceability.md`, `plan.md` and `evidence-q7a.md`. The remaining items are
documentation-level only.

Still not proven: this leg proves a C++/wasm encoder drives the mapped WebGPU operations and that
`rhi.h` compiles for `wasm32` — not that a `doom3::rhi::Device` backend exists, not the T089
positive path, and not a software-adapter path (all correctly recorded NOT RUN/out of scope).
