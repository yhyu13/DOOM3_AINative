# Review: T089 header/WebGPU reconciliation (issue #36)

## Independent reviewer

- **Identity**: an AI subagent reviewing as an independent reviewer. I have no authorship of the
  reviewed files, did not commit or push, did not touch `doom3_orig/doom3`, and modified no tracked
  file other than this review.
- **Worktree and branch**: `.kilo/worktrees/chore-36-header-webgpu-reconcile`, branch
  `chore/36-header-webgpu-reconcile` at `29a1e37`, 10 staged files and no unstaged tracked change.
- **Method**: every claim was treated as untrusted until reproduced. The negative-control mutation
  was built in a scratch copy under `%TEMP%\kilo\t089-scratch` (host temp directory), outside the
  repository; build logs were written to git-ignored `build/`.
- **Reproducibility note**: bare `cmake` and `ctest` are not on this shell's `PATH` (exit 1). I used
  the Visual Studio-bundled binaries the existing cache records at
  `C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\{cmake,ctest}.exe`.

## Scope and commands (worktree root)

| Command | Exit | Observation |
|---|---|---|
| `cmake --build build-rhi-t089 --config Debug` (bundled path) | 0 | Debug build clean; zero warning lines in the log |
| `ctest --test-dir build-rhi-t089 -C Debug --no-tests=error` | 0 | `100% tests passed, 0 tests failed out of 7` |
| `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | `57/57 public declarations classified; statuses, check rows and justifications valid` |
| `py -3 -I scripts/validate-webgpu-mapping.py --self-test` | 0 | `baseline valid, 13 declarations, 14 negative controls detected` |
| `py -3 -I scripts/test-validate-webgpu-mapping.py` | 0 | `14 document mutations and 14 validator mutants behaved as required` |
| `py -3 -I scripts/validate-rhi-contract.py` | 0 | `1 header(s) and 3 public test file(s)`, zero prohibited hits |
| `git diff --cached --check` | 0 | staged whitespace gate clean |
| `node tests/webgpu/run-conformance.mjs --out build/webgpu-q7a-review` | 0 | Chrome/156.0.8078.73: 12 pass / 0 fail / 1 not run (`A1-software-adapter`); issue #36 AC satisfied, contrary to `evidence-t089.md:78` |
| scratch configure / mutant build / mutant `ctest` | 0 / 0 / **8** | `rhi.q1a.clear_readback` fails with `reported_facts=0x3 expected=0x7` and `ASSERTION FAILED: the adapter did not report its identity and memory facts` |
| scratch restore build / first `ctest` | 0 / **8** | copy restored without a newer timestamp, so MSBuild reused the mutant object: the stale-build hazard `evidence-t089.md:58-61` describes reproduces |
| scratch touch source / rebuild / `ctest` | 0 / 0 / 0 | after touching `rhi_d3d12.cpp`, `100% tests passed, 0 tests failed out of 7` |

## Findings

### MAJOR

1. **The new assertion is in the portable public test but asserts a D3D12-only fact.**
   `tests/renderer/rhi_clear_test.cpp:51-59` requires `reportedFacts` to contain all three bits on
   every backend. `webgpu-mapping.md:49` and `:52-55` define a browser backend as
   `reportedFacts = 0` with every numeric field cleared. A compliant WebGPU backend therefore fails
   `rhi.q1a.clear_readback`, the very suite the mapping says should define portable behaviour.
   The assertion is sensitive (see Q5) but mis-scoped; it belongs in `tests/renderer/d3d12/` or must
   be gated on a backend-capability signal. This is the one item I would not sign off as a
   WebGPU-facing acceptance test.

2. **`webgpu-mapping.md` still tells two stories about adapter selection.**
   G4 at `webgpu-mapping.md:138-140` still says `adapterIndex` "has no honest WebGPU source" and
   "the backend must report `Unsupported` for an index request", while the new row at `:76` and the
   contract at `contracts/rhi-foundation.md:92-94` make index 0 portable and reserve `Unsupported`
   for indexes the backend cannot enumerate. G4 was not updated with the new rule and reads as an
   instruction to return `Unsupported` even for index 0. T089 owns this document; the gap section
   contradicts the table it is supposed to summarize.

### MINOR

3. **The contract's absolute header-gate sentence was not amended.** `contracts/rhi-foundation.md:142`
   still says a field with no WebGPU-shaped operation "belongs to the D3D12-private tier or a native
   extension, not to the core", while the new §Optional adapter facts (`:82-101`) adds the
   capability-signal exception. The specific section effectively overrides the general sentence, but
   the prior consistency review (B1) asked for the gate rule itself to be separated; a strict reader
   can still cite `:142` to reject these fields.

4. **`evidence-t089.md:77` overstates the adapterIndex proof.** `rhi.q1a.outstanding_release` uses
   index `9999` (`tests/renderer/rhi_lifetime_test.cpp:310`), which is out of range on this host and
   would return `Unsupported` under the pre-existing range check even without the new rule. It does
   not exercise a nonzero-but-in-range index, so it does not prove "nonzero → `Unsupported` for a
   non-enumerating backend". No test uses index 1.

5. **The "unavailable" half of the `DedicatedMemory` semantics is unexercised by a passing path.**
   Both D3D12 adapters set the bit (software adapter reports value 0), so the only clear-bit run is
   the failing mutant. A client can distinguish "reported zero" from "unavailable" by checking the
   bit, but nothing in the API enforces that check; the distinction is documentation-only.

6. **`evidence-t089.md:25` says "the six existing fields ... keep their names and order".**
   `AdapterInfo` had five fields before this change (the sixth might be `DeviceOptions.adapterIndex`);
   the sentence is ambiguous. No functional impact.

### NOTE — scope

The branch edits `src/renderer/d3d12/rhi_d3d12.cpp`, `tests/renderer/rhi_clear_test.cpp`,
`traceability.md` and the two Python scripts, which are outside issue #36's stated "In scope" list,
and the backend now emits a new public field. This is disclosed in `evidence-t089.md` §Changed
surface and the expanded T089 row, so it is stated, not hidden; but it is a wider branch than the
issue text and the issue's "no D3D12 backend behaviour changes" out-of-scope line.

## Answers to the five questions

1. **Does `AdapterFact` genuinely resolve the violation, or paper over it?** It genuinely removes the
   header/contract contradiction and gives a browser backend defined values. It does not give the
   three fields a WebGPU-shaped operation, and the new `reportedFacts` field itself has none; the
   branch resolves the conflict by amending the rule to allow a capability signal, which the prior
   consistency review (B1) recommended. The strongest argument for moving the three fields to
   `ext/` is that adapter identity has no honest WebGPU source at all and is native-only data, so
   keeping it in the core leaves every WebGPU backend carrying permanently dead fields and every
   client branching on a bit. The chosen option survives that argument only as an explicit contract
   amendment (and with the public-test defect above); it is not a literal satisfaction of the old
   gate wording, and a stricter reviewer could still demand `ext/`.

2. **Is the `DedicatedMemory` semantics coherent?** Yes. Bit set + 0 is a reported zero (DXGI WARP
   really reports 0); bit clear is unavailable, so a client can distinguish them correctly by
   checking the bit. The complexity is moderate and arguably not worth it for one rarely used fact,
   and the clear-bit path is never exercised by a passing backend; the distinction is prose, not
   type-enforced. It is coherent, but only as strong as client discipline.

3. **Is `adapterIndex` now compliant?** At design level, yes: index 0 is portable, and out-of-range
   indexes return `Unsupported` (`rhi_d3d12.cpp:415-420`). The `9999` case only proves the existing
   range check, not the new "cannot enumerate → `Unsupported` for nonzero" rule. D3D12 can enumerate,
   so it may still accept index 1 when a second adapter exists; that is consistent with "the core
   only guarantees 0" but conflicts with a flat "nonzero → `Unsupported`" reading of the contract.

4. **Is `ReleaseStatus`/`TryRelease` a core lifetime rule or a redefinition?** It is a defensible
   reclassification, not a native passthrough: WebGPU has `onSubmittedWorkDone`, so completion can be
   observed and the three outcomes derived by bookkeeping, and the contract now states this
   explicitly. It still has no GPU release operation, and the mapping row admits "nothing is released
   on the GPU timeline"; the gate is correspondingly weaker, but the behaviour is honest and
   implementable.

5. **Is the new test assertion sensitive?** Yes to the bit being absent: the `0x3` mutant fails with
   the exact message, and the touched restore returns 7/7 (commands above). It does not validate the
   field values or reject extra bits, and it is D3D12-only, which is MAJOR-1.

## Decision

**Accept with limitations.** The capability-signal reconciliation is coherent, the contract and the
header now state the same rule, and the D3D12 evidence reproduces (7/7, exact mutation failure,
57/57 mapping, browser spike 12/0/1). Do **not** treat `rhi.q1a.clear_readback` or
`webgpu-mapping.md` as WebGPU-ready until MAJOR-1 (scope the `0x7` assertion to D3D12 or gate it on
the capability bit) and MAJOR-2 (update G4) are fixed. Unresolved: the contract's absolute gate
sentence at `contracts/rhi-foundation.md:142` and the untested clear-bit path.

**Still not signed off:** the public-seam test asserting `0x7` while the mapping defines a browser
backend as `0`.

## Second pass (2026-10-29) — supersedes the first-pass decision

The author reports fixing both MAJOR findings and two defects the first-pass control exposed. I
re-read the staged tree (10 changed files plus this review), treated every fix as a claim, and
reproduced it. No commit, push, submodule touch, download, or tracked-file edit outside this review.

### What changed and what I verified

- **MAJOR-1:** the `0x7` native assertion moved to `tests/renderer/d3d12/rhi_d3d12_test.cpp:32-40`
  (`RunStateJournal`). The public test (`tests/renderer/rhi_clear_test.cpp:53-71`) now asserts only
  the portable invariant: no undefined bit, and a set `VendorId`/`DeviceId` bit implies a non-zero
  id. `SubmitAndWait` now accumulates (`*ok &=`, `rhi_d3d12_test.cpp:20,24`).
- **MAJOR-2:** G4 (`webgpu-mapping.md:138-142`) no longer says any index is unsupported; it now
  describes `reportedFacts` clear bits, index 0 and a browser backend that is compliant because the
  caller must treat a clear bit as unavailable.
- **Contract gate:** `contracts/rhi-foundation.md:142` now contains the exception "unless the core
  itself defines how the absence is reported (§Optional adapter facts and adapter selection)".
- **Validator parser:** `scripts/validate-webgpu-mapping.py:44` accepts an optional underlying type;
  the fixture has `enum class Mode : uint32_t`, and the `scoped-enum-detection` mutant is rejected.
- **Lexical gate:** the public comment now says "the private test directory"
  (`rhi_clear_test.cpp:53`) and `validate-rhi-contract.py` passes with zero hits over 1 header and 3
  public test files.

### Commands and exits (worktree root; bundled CMake/CTest path as in the first pass)

| Command | Exit | Observation |
|---|---|---|
| `cmake --build build-rhi-t089 --config Debug` | 0 | clean; no warning lines |
| `ctest --test-dir build-rhi-t089 -C Debug --no-tests=error` | 0 | `100% tests passed, 0 tests failed out of 7` |
| `build-rhi-t089/Debug/q1a_d3d12_test.exe state_journal` | 0 | `PRIVATE_ADAPTER reported_facts=0x7 expected=0x7`, PASS on both adapters |
| `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | `57/57 public declarations classified` |
| `py -3 -I scripts/validate-webgpu-mapping.py --self-test` | 0 | `13 declarations, 14 negative controls detected` |
| `py -3 -I scripts/test-validate-webgpu-mapping.py` | 0 | `14 document mutations and 14 validator mutants behaved as required`; `scoped-enum-detection` rejected |
| `py -3 -I scripts/validate-rhi-contract.py` | 0 | header scan 1 header + 3 public tests, zero prohibited hits |
| `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | injected violations detected |
| `git diff --cached --check` | 0 | clean |
| `node tests/webgpu/run-conformance.mjs --out build/webgpu-q7a-review2` | 0 | Chrome/156.0.8078.73: 12 pass / 0 fail / 1 not run (`A1-software-adapter`); issue #36 AC still holds |

Scratch controls (`%TEMP%\kilo\t089-scratch2`, fresh copy, no tracked edits):

| Control | Exit | Observation |
|---|---|---|
| `reportedFacts &= ~DedicatedMemory` → `0x3`; build; full ctest | 0; **8** | 1/7 failed: `rhi.q1a.state_journal`; `rhi.q1a.clear_readback` **passed**; `PRIVATE_ADAPTER reported_facts=0x3 expected=0x7` and `ASSERTION FAILED: the native adapter did not report its identity and memory facts` |
| `reportedFacts = 0` (browser-shaped); build; full ctest | 0; **8** | `rhi.q1a.clear_readback` **passed** with `ADAPTER reported_facts=0x0 known=0x7`; `state_journal` failed on the private `0x7` assertion. This is the direct proof of the MAJOR-1 split. |
| With `0x3`, restore the old `*ok = Require(...)` in the scratch private test; build; run `state_journal` | 0; 0 | **the test passed although the assertions above it failed** — the defect the author reports is real |
| Restore `*ok &=`; rebuild | 0; 8 expected | `state_journal` fails again on `0x3` (the fix is what preserves the earlier failure) |
| Restore original backend/test, touch both sources, rebuild, full ctest | 0; 0 | `100% tests passed, 0 tests failed out of 7` |

### Remaining findings

**BLOCKING / MAJOR: none.** The two first-pass MAJORs are fixed and reproduced as fixed, including
the case where a backend reports `reportedFacts = 0`: the public suite passes and only the private
native assertion fails.

**MINOR**

1. **The `9999` evidence still overstates the adapterIndex proof.** `evidence-t089.md:41-42,86`
   still says a "nonzero `adapterIndex`" is covered by the `9999` case, but `9999` is out of range on
   this host and exercises the pre-existing range check (`rhi_d3d12.cpp:415-420`); no test uses an
   in-range nonzero index. The claim should say "out-of-range index" rather than "nonzero index", or
   add an index-1 control.
2. **G4's "any other index returns `Unsupported`" is unqualified.** `webgpu-mapping.md:138-141`
   reads as a universal rule, while the contract (`:92-94`) and the header bind it to a backend that
   cannot enumerate; the D3D12 backend accepts index 1 when a second adapter exists
   (`rhi_d3d12.cpp:415-422`). Qualify it as "a browser/non-enumerating backend".
3. **The public suite enforces an invariant the contract does not state.** `rhi_clear_test.cpp:64-71`
   requires a set `VendorId`/`DeviceId` bit to imply a non-zero value, but
   `contracts/rhi-foundation.md:82-101` only defines the clear-bit = zero/unavailable direction.
   Device id 0 can be a real id; either state the non-zero invariant in §Optional adapter facts or
   relax the `DeviceId` check.

**LIMITATION (the one material item I would still not sign off as executed evidence)**

The WebGPU clear-bit path is still **NOT RUN on a WebGPU backend**. The public suite's `0x0` case is
verified by construction and by the D3D12 `reportedFacts = 0` proxy above, not by a passing browser
backend; the Emscripten encoder leg remains NOT RUN in T088. That is the only material limitation;
the three MINORs are editorial/evidence wording and should ride along before any Q7/WebGPU claim. The
branch's wider-than-issue scope (backend/test/validator edits) remains disclosed in
`evidence-t089.md` and is not a blocker for the D3D12 Q1a claim.

### Second-pass decision

**Accept with limitations.** No BLOCKING or MAJOR finding remains. The capability-signal
reconciliation is coherent, the contract and header now agree, the native `0x7` assertion is
correctly private, a `0x0` backend passes the public suite, and the `&=` defect is proven and fixed.
The only material limitation is the unexecuted WebGPU clear-bit path; the three MINOR evidence/wording
items above are non-blocking.
