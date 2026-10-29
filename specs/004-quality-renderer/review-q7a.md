# Review: Q7a WebGPU mapping gate and conformance spike (issue #35)

> **Status: superseded.** The first-pass "changes requested" decision below was resolved; the current decision is in §Second-pass review (accepted with limitations).

## Independent reviewer

- **Identity**: an AI subagent reviewing as an independent reviewer. I have no authorship of the
  reviewed files, did not commit, push, or touch `doom3_orig/doom3`, and modified no tracked file
  other than this review.
- **Worktree and branch**: repository `DOOM3_AINative`, worktree
  `.kilo/worktrees/docs-35-webgpu-conformance`, branch `docs/35-webgpu-conformance`. The branch tip
  at review time was still the base revision `d9bf1ca`; all reviewed files were uncommitted working
  tree changes (`M specs/004-quality-renderer/tasks.md` plus the new files).
- **Method**: every claim was treated as untrusted until reproduced. Falsification used temporary
  copies outside the repository and the ignored `build/` directory only.

## Scope and commands rerun

All commands were run from the worktree root. Exit codes were captured immediately.

| Command | Exit | Observation |
|---|---|---|
| `node tests/webgpu/run-conformance.mjs --repeat 2 --out build/review-q7a` | 0 | Chrome/156.0.8078.73, 12 pass / 0 fail / 0 not run on both attempts, `repeatable across 2 attempt(s): true`; 548 ms and 576 ms |
| `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | `OK: 52/52 public declarations mapped; statuses valid; checks valid` |
| `py -3 -I scripts/validate-webgpu-mapping.py --self-test` | 0 | `self-test: baseline valid, 10 declarations, 12 negative controls detected` |
| `py -3 -I scripts/test-validate-webgpu-mapping.py` | 0 | 12 document mutations and 12 validator mutants behaved as required |
| `py -3 -I scripts/validate-rhi-contract.py` | 0 | `header scan: RUN over 1 header(s) and 3 public test file(s)`, zero prohibited hits |
| `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | required by issue #35 but absent from the evidence command table; it passes |
| `git status --porcelain` | 0 | `M specs/004-quality-renderer/tasks.md` plus the expected untracked files; submodule clean at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`. A separate untracked `review-spec-consistency.md` appeared later from a concurrent reviewer; it is outside this review and was not modified. |
| `git diff --check` | **2** | `specs/004-quality-renderer/tasks.md:202: new blank line at EOF` |
| `node --version` / `py -3 --version` | 0 | v24.19.0 / Python 3.14.4, matching the evidence |
| `ctest --test-dir build-rhi-q7a -C Debug --no-tests=error` | not run | `ctest` is not on this shell's `PATH`; the retained `ctest-junit.xml` records `tests="7" failures="0"` |

The 12 in-page checks do reproduce on real hardware: the observed adapter is `intel` / `xe-lpg`,
the readbacks are byte-exact, and the retained PNG shows the result table and the orange
`[0,128,255,255]` canvas. The problems below are evidence-integrity and gate-scope problems, not
"the spike did not run".

## Findings

### BLOCKING

1. **`git diff --check` does not pass, contrary to the evidence.** `evidence-q7a.md:68` records exit
   0 with "whitespace gate"; the actual command exits 2 because `specs/004-quality-renderer/tasks.md`
   gained blank lines at EOF (`tasks.md:202`). The three trailing CRLFs are visible in the raw bytes,
   and the diff also changed line 1 by inserting a UTF-8 BOM, an unintended encoding change beyond
   the permitted "T088 status annotation only". The untracked review files have the same defect:
   diffing each as an addition reports a new blank line at EOF
   (`tests/webgpu/conformance.html`, `tests/webgpu/run-conformance.mjs`,
   `scripts/validate-webgpu-mapping.py`, `scripts/test-validate-webgpu-mapping.py`,
   `specs/004-quality-renderer/evidence-q7a.md`). Fix: strip the trailing blank lines from every new
   file, restore `tasks.md` to no BOM with only the intended annotation lines, then rerun
   `git diff --check` *and* `git diff --cached --check` after staging (or `git add --intent-to-add`)
   so the new files are actually covered, and record that output instead.

2. **A machine-specific absolute path is retained in the tracked evidence.**
   `specs/004-quality-renderer/evidence/q7a-conformance.json:6` stores `browser.executable` as a
   `C:`-rooted Chrome installation path. This violates the repository non-negotiable "no
   machine-specific absolute paths in tracked files" and the evidence template's "never include
   machine-specific absolute paths in tracked records". `run-conformance.mjs:265` writes this field
   directly, so scrubbing only the JSON is insufficient: a later `--evidence` run reintroduces it.
   The existing absolute-path control in `validate-rhi-contract.py` does not cover this file, because
   the new evidence record and JSON are not in its scanned file list. Fix: write only the executable
   basename plus the browser version into the portable report (keep the full path in the ignored
   `run.log`), scrub the retained JSON, and extend the absolute-path scan to the Q7a evidence
   Markdown/JSON or explicitly record why that file is out of scope.

### MAJOR

3. **The recorded run timings do not match the retained artifact.** `evidence-q7a.md:60` records
   "621 ms and 602 ms", while the tracked `evidence/q7a-conformance.json` and the ignored
   `build/webgpu-q7a/results.json` both record 570 ms and 598 ms. A reviewer cannot reconcile the
   quoted observation with either retained result. Fix: quote the retained run, or retain the run
   that produced 621/602 ms and label which artifact belongs to which attempt.

4. **The evidence claims a semantic mapping guarantee that the gate does not provide.** The opening
   claim at `evidence-q7a.md:3-4` ("fully mapped ... with every unmappable field justified") and the
   `tasks.md:176` phrase "the semantic field-to-WebGPU mapping gate" overstate the control. The
   validator's own docstring (`scripts/validate-webgpu-mapping.py:10-11`) says it "cannot judge
   whether a mapping is technically correct"; `evidence-q7a.md:98` nevertheless treats 52/52 as
   proof of mapping completeness. I demonstrated that the validator accepts all of the following:
   flipping `AdapterInfo.dedicatedVideoMemory` and `AdapterInfo.deviceId` from `unsupported` to
   `mapped` with an invented operation; changing `ReadbackResult.rowStride`'s check reference from
   C6 to C1 or N1; replacing an `unsupported` justification with `todo`; deleting the N1–N3 rows
   from the checks table; and changing the C6 checks-table row to "nothing was executed / not run".
   Fix: state that the gate is structural and that semantic correctness is reviewer judgement (this
   review), and/or harden the validator with a hardcoded set of known-unmappable declarations that
   must remain `unsupported`, make N1–N3 required rows, and check that each row's check reference is
   in a declared per-row set rather than any valid ID.

### MINOR

5. **`AdapterClass.Hardware` is `mapped` while its parent `AdapterClass` is `mapped-with-gap`**
   (`webgpu-mapping.md:30-31`). `adapter.info.isFallbackAdapter === false` does not prove hardware
   (a non-fallback software implementation can report false); the row's justification is an
   observation, not a mapping proof. Mark it `mapped-with-gap` or justify the equality explicitly.
   (I confirmed that in Chrome 156 the property is on `GPUAdapterInfo`, not `GPUAdapter`, and is
   really `false` here, so the observation itself is real.)

6. **Several check references do not exercise the row they are attached to.**
   `Device.IsComplete` (`webgpu-mapping.md:50`) and `Submission` (`:78`) cite C8, which is a
   device-loss check and never calls `onSubmittedWorkDone` or creates a submission;
   `ReadbackResult.adapter` and `Status.Ok` cite C1, which records adapter identity but not a
   readback result or a completed promise; `Status.InvalidArgument` cites E, which only proves that
   *no* uncaptured error occurred and never asserts that a scoped validation error was classified.
   These are mapping-only rows and should cite `—`, or a check that actually performs the operation.

7. **N2 is not an independent negative control and does not prove pitch validation.**
   `conformance.html:322-327` computes `packedWouldBeWrong` from the same C6 buffer (offset 16 is
   `0xAA` padding, offset 256 is the row), so it cannot fail while C6 passes. C6 proves that the
   driver places rows at the requested 256-byte pitch and leaves padding untouched; it never asks
   the implementation to reject a non-multiple pitch. The checks-table wording "packed offset
   rejected" (`webgpu-mapping.md:96,100`) should say "the packed offset does not contain row 1",
   and a real negative control should attempt `bytesPerRow: 16` and require a validation error.

8. **The UNORM comment and evidence claim round-half-to-even, but the code uses half-up.**
   `conformance.html:95` says "round-half-to-even" while `unorm8` calls `Math.round`;
   `evidence-q7a.md:52-54` repeats the claim. The only tie exercised is `0.5 * 255 = 127.5`, where
   both rules give 128, so no result changes, but the comment and evidence are inaccurate.

9. **The runner trusts the page and would not notice a hardcoded PASS.** I replaced the page with a
   page that never performs GPU work and sets `window.__conformanceResult` to twelve PASSes; the
   unchanged runner exited 0 on two attempts, reported `repeatable ... true`, and wrote both
   artifacts. The page itself is not vacuous — C1 requires an adapter and device, and every later
   check is inside a try/catch that records FAIL on a throw — but the runner has no independent GPU
   assertion or tamper evidence. State this limitation in the evidence; optionally require non-null
   `observed` evidence and an adapter identity in the runner, or hash the page.

10. **An explicit `--browser` path is not enforced.** `run-conformance.mjs:63-75` filters the
    candidates and returns the first existing browser, so a reviewer who pins a nonexistent path
    silently gets the next installed Chrome/Edge. Fail instead of falling back when `--browser` was
    supplied, so the recorded executable is the one that was requested.

11. **The software-adapter reason is not produced by any retained command.**
    `evidence-q7a.md:102` and `webgpu-mapping.md:142` say `forceFallbackAdapter: true` returned no
    adapter, but `conformance.html:104-109` defines that path and only calls
    `createDevice(false)` (`:142`, `:367`). The NOT RUN status is honest; the stated reason is an
    unretained observation. Either retain a probe command/artifact or say "not attempted".

12. **The "every public declaration" count is not literal.** `validate-webgpu-mapping.py` collapses
    the three `ToString` overloads (`rhi.h:143-145`) into one name and skips constructors,
    destructor and deleted operators (`:105-111`), so 52/52 counts names/fields, not every C++
    declaration. Either word the claim as "every mapped field and named operation" or count
    signatures and document the exclusions.

13. **The `vendorId` adaptation points at a table that is not in the repository.**
    `webgpu-mapping.md:38` says "Requires a documented string-to-id table"; no such table or
    heuristic is included or referenced. The `AdapterClass` inference at `:30` is likewise
    unspecified. Document the table/heuristic or make the row `unsupported` until it exists.

14. **The evidence omits the issue's `validate-rhi-contract.py --self-test` command and the
    submodule SHA/fixture hash the template asks for.** The table at `evidence-q7a.md:58-68` records
    the plain gate but not `--self-test` (issue acceptance criterion 3), and the "Source identity"
    section (`:11-14`) says the submodule is untouched/clean without its SHA; the "Data identity"
    section has no fixture hash. Add the SHA `a9c49da5...` and a synthetic-file hash, and record the
    self-test exit.

15. **Nit: the local server's containment check is a bare string prefix.**
    `run-conformance.mjs:83` uses `file.startsWith(root)` without a path separator; the URL parser
    makes traversal hard, but `file === root || file.startsWith(root + path.sep)` is the correct
    check. Also, `repeatable` (`:256-261`) compares per-check statuses only, not observed bytes, so
    the label says less than it appears to.

16. **Nit: the "shipped in the same commit" claim is forward-looking.** `evidence-q7a.md:11-14`
    says the record ships in the same commit as the listed files, but at review time the branch tip
    is still the base revision and everything is uncommitted. This is acceptable once committed;
    until then it should not be read as a verified identity.

### Verified as correct (no finding)

- `bytesPerRow % 256 == 0` is the rule for texture/buffer copy commands; `writeTexture` is more
  relaxed, so C6's 16-byte `writeTexture` layout is legal and the 256-byte `copyTextureToBuffer`
  path is the one being tested.
- `mapAsync(READ)` + `getMappedRange()` + `slice()` + `unmap()` is the right caller-owned-bytes
  lifetime, and the page performs the copy before unmapping.
- `device.lost` reasons `destroyed`/`unknown`, the absence of a per-submission completion object,
  and the queue-wide nature of `onSubmittedWorkDone()` are stated correctly.
- The adapter-info fields (`vendor`, `architecture`, `device`, `description`,
  `isFallbackAdapter`) and the canvas `RENDER_ATTACHMENT | COPY_SRC` usage match the observed
  Chrome 156 behavior and the current WebGPU surface.
- `clearValue` being one value per attachment, so that a per-row pattern needs `writeTexture` or a
  position-reading draw, is correct.
- The C1/C4/C6/C7 observed values reproduce exactly.

## Falsification attempts and their results

### Header mutation (temporary copy of `rhi.h`)

| Mutation | Mapping gate | Lexical header gate |
|---|---|---|
| Add `uint64_t fenceValue` to public `ReadbackResult`, original doc | exit 1, `declaration not mapped: ReadbackResult.fenceValue` | caught: `fence/fenceValue` |
| Same, with an invented `ReadbackResult.fenceValue` row added to a temporary doc | **exit 0** | caught: `fence/fenceValue` |
| Add `committedResource` | only structural (row missing unless fabricated) | **not caught** |
| Add `cmdQueue` | only structural | **not caught** |
| Add `gpuDescriptorHandle` | only structural | **not caught** |
| Add `stagingPitch` | only structural | **not caught** |
| Put `RootSignature` / `fence` in a public comment | no declaration change | **caught** (the gate scans comments too) |

The mapping gate is not vocabulary-aware; if a fabricated row is added, a D3D12 noun in a public
declaration is accepted. The lexical gate catches the spellings in its list but not the synonyms the
contract itself lists (`GPUDescriptorHandle`, `CmdQueue`, `RootParameter`, etc.).

### Mapping-document mutation (temporary copies)

| Mutation | Result |
|---|---|
| `AdapterInfo.dedicatedVideoMemory` `unsupported` → `mapped`, invented operation, blank justification | **accepted (exit 0)** |
| `AdapterInfo.deviceId` `unsupported` → `mapped` | **accepted** |
| Delete the entire conformance-checks table | rejected (exit 1, 9 problems) |
| Rename `AdapterInfo.deviceId` row to the nonexistent `AdapterInfo.deviceID` | rejected (missing + stale) |
| Change `ReadbackResult.rowStride` check C6 → C1 | **accepted** |
| Change the same check to N1 | **accepted** |
| Replace an `unsupported` justification with `todo` | **accepted** |
| Change the C6 checks-table row to "nothing was executed / not run" | **accepted** |
| Delete the N1–N3 rows from the checks table | **accepted** (only C1–C8 are required) |
| Rename `## Gaps and deltas` to `## Gaps` | **accepted** (the check is a prefix match) |

So the gate catches structure (missing/stale/duplicate/short rows, unknown statuses/IDs, no table,
no separator, no `## Gaps` prefix) and does not catch semantic falsehood.

### Runner trust

A temporary page that does no GPU work and hardcodes twelve PASSes produced exit 0, two attempts,
`repeatable: true`, a `results.json`, and a screenshot. The runner cannot distinguish that from the
real page. The real page is not vacuous: it needs an adapter/device and records FAIL on any thrown
operation. The runner should not be described as independent verification of GPU execution.

### C6/N2

C6 genuinely verifies that row 0 and row 3 land at 256-byte offsets, that rows 1 and 2 also match
(all four rows are compared, not just 0 and 3), and that the padding bytes remain `0xAA`. N2 is
derived from the same buffer and is not a second control; it does not attempt a packed-pitch copy.
The documentation's "packed offset rejected" wording overstates what N2 proves.

### WebGPU API spot-check

A temporary page against the same Chrome reported `adapter.info.isFallbackAdapter === false` and
`adapter.isFallbackAdapter === undefined`, confirming that the page reads the right property for
Chrome 156. `adapter.info` exposes `vendor`/`architecture`/`device`/`description`; the empty
`description` observed here matches the mapping's fallback justification.

## Decision (first pass — superseded)

**First pass — changes requested.** The 12 GPU checks reproduce and the mapping table is mostly technically
sound, but the record cannot be accepted as evidence while the whitespace gate fails, the tracked
evidence contains a machine-specific absolute path, the quoted timings contradict the retained
artifact, and the "semantic mapping" claim is stronger than the structural gate supports.

Remaining unresolved items (legitimately outside this review or this host):

- The Emscripten/Dawn encoder leg is NOT RUN; Q2 remains blocked. This is correctly labelled.
- The software WebGPU adapter leg is NOT RUN; its stated reason is not retained (finding 11).
- Out-of-memory classification is mapping-only; no OOM was induced.
- The supporting CTest run could not be rerun here because `ctest` is not on the review shell's
  `PATH`; only the retained JUnit file (`7` tests, `0` failures) was inspected.
- No claim that Q2 or Q7 is satisfied appears in the reviewed documents; that was verified.

## Second-pass review (2026-10-29)

The first-pass BLOCKING and MAJOR findings are fixed and independently reproduced. No BLOCKING or
MAJOR finding remains. The remaining items are documentation limitations and minor robustness nits;
none invalidates the executed Q7a evidence or the claim that the mapping field is classified rather
than "semantically proven" by the script. The current decision is recorded under §Final decision.

### Re-verified after the author's changes

| Command | Exit | Observation |
|---|---|---|
| `node tests/webgpu/run-conformance.mjs --repeat 2 --out build/review-q7a2` | 0 | 12 pass / 0 fail / 0 not run on both attempts; `repeatable across 2 attempt(s): true`; 577 ms and 546 ms |
| `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | `OK: 52/52 public declarations classified; statuses, check rows and justifications valid` |
| `py -3 -I scripts/validate-webgpu-mapping.py --self-test` | 0 | `baseline valid, 10 declarations, 14 negative controls detected` |
| `py -3 -I scripts/test-validate-webgpu-mapping.py` | 0 | 14 document mutations and 13 validator mutants behaved as required |
| `py -3 -I scripts/validate-rhi-contract.py` | 0 | `header scan: RUN over 1 header(s) and 3 public test file(s)`, zero prohibited hits |
| `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | passes; still absent from the evidence command table (finding 10 below) |
| `git diff --cached --check` after `git add -A` | 0 | no whitespace errors; staged version covers the new files |
| `git status --porcelain` | 0 | 16 staged paths, including Q7a and some Q7a-adjacent documentation updates (finding 12) |

- **Whitespace**: `git diff --cached --check` exits 0. `tasks.md` starts with `23 20 54` (no BOM)
  and ends with exactly `73 2E 0D 0A`; the Q7a HTML, JS, Python and Markdown files each end with one
  line terminator. The first-pass blank-line-at-EOF failure is gone.
- **Machine-specific path**: the retained JSON now has
  `"executableName": "chrome.exe"` and no drive-rooted path; the full Chrome path appears only in
  the ignored `build/webgpu-q7a/run.log`. A scan of every staged file under `specs/`, `scripts/`
  and `tests/` found no drive-rooted filesystem path (the only colon-separator hits are `http://`
  URLs and Python string literals containing escaped newlines). The first-pass BLOCKING path
  finding is gone.
- **Timings**: `evidence-q7a.md:59` records 590 ms and 525 ms; the retained
  `evidence/q7a-conformance.json` records exactly `590` and `525`, both with `pass=12 fail=0
  notRun=0`. The first-pass MAJOR mismatch is gone.
- **Nonce control**: removing `runNonce` from a temporary copy of the page makes the runner print
  `the page did not echo the run nonce ...` and exit 2; the unmodified page exits 0. The control is
  real. It is not tamper-proof: a temporary page that hardcodes twelve PASSes **and** reads
  `?run=` from `location.search` exits 0 (I reproduced this), so the guard catches an accidental
  missing echo, not an adversarial page. This is a limitation, not a defect in the recorded run.
- **C5 fix**: `tests/webgpu/conformance.html:231` now tests `.equal`, not the comparison object.
  In a temporary copy, changing the expected compute literal from `i*3+1` to `i*3+2` makes
  `C3-bind-group` and `C5-compute` both FAIL and the runner exit 1. Every `bytesEqual` call site
  now uses `.equal` (C2, C4, C5, C6, C7, N1); no other check has the same truthiness hazard.
- **G8 and T089**: the five `unsupported` declarations named in `webgpu-mapping.md:145-153` match
  the five `unsupported` rows in the mapping table. `tasks.md:186` defines T089 once, and the
  traceability/plan/quickstart notes are consistent with Q7a being partial and Q2 blocked.
- **Validator hardening**: N1–N3 and E are now required (`validate-webgpu-mapping.py:32`),
  placeholder justifications are rejected (`:239-240`), and the self-test/mutation suites exercise
  the new rules (14 and 13 cases). The remaining inability to detect a false `mapped` status or a
  semantically wrong check reference is now stated explicitly in
  `webgpu-mapping.md:26-36` ("What this gate cannot decide") and in the evidence.

### Remaining findings (MINOR / NIT, documentation or robustness; no BLOCKING/MAJOR)

1. **MINOR — `AdapterClass.Hardware` is still `mapped` while its parent is `mapped-with-gap`**
   (`webgpu-mapping.md:42-43`). `isFallbackAdapter === false` is an inference, not a direct
   implementation of "hardware"; mark the row `mapped-with-gap` or narrow the claim.
2. **MINOR — several check references are still aspirational.** `Device.IsComplete` and
   `Submission` cite C8 (`webgpu-mapping.md:62,90`), but C8 exercises `device.lost`, not
   `onSubmittedWorkDone` or a submission token; `ReadbackResult.adapter` and `Status.Ok` cite C1
   (`:73,87`), which records adapter identity but not a readback result or a resolved promise;
   `Status.InvalidArgument` cites E (`:85`), which only proves no uncaptured error occurred. The
   document now says check references are not semantically validated, so this is a limitation of
   the table, not a hidden claim; using `—` for mapping-only rows would be more honest.
3. **MINOR — N2 is still derived from the C6 readback, not an independent pitch-validation
   control.** `conformance.html:323-328` computes `packedWouldBeWrong` from the same bytes, and the
   checks table still says "packed offset rejected" (`webgpu-mapping.md:108,112`; evidence line 93).
   The mapping doc now correctly explains that an invalid packed-pitch copy would raise a validation
   error and defeat E, so this is documented; soften the table wording to "the packed offset does
   not contain row 1".
4. **NIT — the `unorm8` comment still claims round-half-to-even while using `Math.round`**
   (`conformance.html:93-95`). The only tie used is 0.5, where both rules give 128, and the
   evidence no longer repeats the claim; fix the comment.
5. **MINOR — the nonce guard is not tamper-evident** (see above; `run-conformance.mjs:198-201` and
   the page's run-nonce echo at `conformance.html:124`). A hardcoded page that echoes the query
   passes. The error message "a static page cannot stand in" is stronger than the control proves.
6. **MINOR — a genuine page crash is reported as a setup failure.** The catch path at
   `conformance.html:418-426` omits `runNonce`, so the runner's nonce check throws and exits 2
   ("the page did not echo the run nonce") instead of exiting 1 with the harness/NOT RUN checks.
   Include the nonce in the catch `env`, or classify a reported crash as a failed check (exit 1).
7. **MINOR — `--browser` is still not enforced.** `run-conformance.mjs:63-75` falls through to the
   next installed Chrome/Edge when an explicit path does not exist, so a pinned reviewer binary can
   silently be replaced. Fail when `--browser` was supplied and does not exist.
8. **NIT — local server containment and repeatability labels.** `run-conformance.mjs:83` uses a
   bare `startsWith(root)` without a separator, and `:260-261` compares per-check statuses only, not
   observed bytes, so `repeatable` says less than a reader may assume.
9. **MINOR — the software-adapter reason is still not retained.** `evidence-q7a.md:104` and
   `webgpu-mapping.md:163-165` say `forceFallbackAdapter: true` returned no adapter, but
   `conformance.html:104-109` defines that path and only calls `createDevice(false)` (`:143`,
   `:368`). The NOT RUN status is honest; the reason is an unretained observation. Retain a probe
   or say "not attempted".
10. **MINOR — the evidence command table still omits `validate-rhi-contract.py --self-test`**
    (`evidence-q7a.md:57-67`), although issue #35's acceptance criterion names it. It passes when
    run (exit 0); add the row.
11. **NIT — the template's identity fields are still thin.** `evidence-q7a.md:13-17` gives no
    submodule SHA or synthetic-fixture hash; add `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` and a
    content hash.
12. **MINOR (scope) — the staged change set is wider than issue #35's in-scope list.**
    `git diff --cached --stat` includes `plan.md`, `quickstart.md`, `traceability.md`,
    `evidence-q2-design.md`, `review-q2-design.md` and `review-spec-consistency.md` in addition to
    the Q7a files. These edits are Q7a-related documentation updates, but they are outside the
    issue's declared paths; separate them or confirm they belong to this change before the PR.

### Unresolved legs

The Emscripten/Dawn encoder leg, the software WebGPU adapter, the `wasm32` compile and OOM remain
NOT RUN and are correctly labelled; Q2 stays blocked. The supporting CTest suite was not rerun here
because `ctest` is not on this shell's `PATH`; the retained JUnit still records 7 tests, 0 failures.
No Q2/Q7 claim was found in the documents.

## Final decision

**Accept with limitations.** The Q7a evidence and mapping are reproducible, the first-pass
blocking/major defects are fixed, and the remaining items are documentation limitations and minor
robustness nits (items 1–12 above), not defects in the executed conformance result. The Q7a work
should not be presented as closing Q2 or Q7; the encoder leg and T089 remain open.
