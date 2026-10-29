# T089 evidence: portable header reconciled with the WebGPU mapping gate

**Stage and claim**: spec 004 task T089, issue #36. Claim: the five declarations that the Q7a mapping
gate found without a WebGPU-shaped operation now carry an explicit availability rule, so the portable
core and the contract no longer contradict each other. The chosen option is the capability signal, not
moving the fields: `AdapterFact` plus `AdapterInfo.reportedFacts` express "this backend cannot report
the fact", the contract's header-gate rule and a new §Optional adapter facts and adapter selection
define the semantics, the three D3D12 fields are set to reported, and the mapping rows changed from
`unsupported` to `mapped-with-gap` with defined browser behaviour.

This record does **not** claim the Emscripten encoder leg (still NOT RUN in T088), any Q7/WebGPU
support, or a rerun of the browser conformance spike: the spike's subject — WebGPU operation behaviour —
did not change and its owner stopped that work.

## Identity and scope

- **Source identity**: base `29a1e37` (`origin/main` after PR #37), branch
  `chore/36-header-webgpu-reconcile`. `doom3_orig/doom3` untouched.
- **Data identity and rights**: synthetic only; no game data or third-party asset.
- **Changed surface**: `src/renderer/rhi/rhi.h` (`AdapterFact`, `AdapterInfo.reportedFacts`, comments on
  `adapterIndex` and `ReleaseStatus`), `src/renderer/d3d12/rhi_d3d12.cpp` (sets the three bits),
  `tests/renderer/rhi_clear_test.cpp` (portable invariant), `tests/renderer/d3d12/rhi_d3d12_test.cpp`
  (native `0x7` assertion, and `SubmitAndWait` now accumulates failures), the contract, the mapping
  document, the mapping validator and its suite, `tasks.md`, `traceability.md`.
- **Exclusions**: no change to clear/readback, submission or release semantics; no new dependency; no
  ABI removal — one enum and one field were added, existing field names and order are unchanged.

## Environment and configuration

- Windows 11 x64, Visual Studio 2022 Professional C++ tools (bundled CMake/CTest, MSVC x64), Debug.
- Adapters exercised: `Intel(R) Graphics` (hardware, vendor `0x8086`, device `0x7D67`) and
  `Microsoft Basic Render Driver` (software, vendor `0x1414`, device `0x008C`). Both report
  `reportedFacts = 0x7`, including a reported zero dedicated memory on the software adapter.
- Python 3.14.4 via `py -3` for the validators. No browser or WebGPU adapter is involved.

## Procedure and retained artifacts

- **Predeclared expectation**: the native backend reports `VendorId | DeviceId | DedicatedMemory`
  (`0x7`) for both D3D12 adapters, asserted only in `tests/renderer/d3d12/`; the public suite asserts
  only the portable invariant (no undefined bits; a set bit implies a non-zero id) so a WebGPU backend
  reporting `0` also passes. An out-of-range `adapterIndex` keeps returning `Unsupported` (covered by
  `rhi.q1a.outstanding_release` with index 9999); the case proves the refusal, not a general rule about
  nonzero indices, because the native backend does enumerate adapters.
- **Commands and exits** (worktree root):

  | Command | Exit | Observation |
  |---|---|---|
  | `cmake -S tests/renderer -B build-rhi-t089 -G "Visual Studio 17 2022" -A x64` | 0 | fresh directory |
  | `cmake --build build-rhi-t089 --config Debug` | 0 | no new warnings in the changed files |
  | `ctest --test-dir build-rhi-t089 -C Debug --no-tests=error --output-junit build-rhi-t089/ctest-junit.xml` | 0 | `100% tests passed, 0 tests failed out of 7` |
  | `build-rhi-t089/Debug/q1a_d3d12_test.exe state_journal` | 0 | `PRIVATE_ADAPTER reported_facts=0x7 expected=0x7` for both adapters |
  | `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | `57/57 public declarations classified` |
  | `py -3 -I scripts/validate-webgpu-mapping.py --self-test` | 0 | 14 negative controls detected |
  | `py -3 -I scripts/test-validate-webgpu-mapping.py` | 0 | 14 document mutations and 14 validator mutants behaved as required |
  | `py -3 -I scripts/validate-rhi-contract.py` | 0 | `header scan: RUN over 1 header(s) and 3 public test file(s)`, zero prohibited hits |
  | `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | all injected violations detected |
  | `git diff --cached --check` | 0 | staged whitespace gate |
- **Retained artifacts**: build and CTest logs plus JUnit in the ignored `build-rhi-t089/`.
- **Negative controls**:
  1. Backend mutation — mask out `DedicatedMemory` so `reportedFacts` becomes `0x3`: `rhi.q1a.state_journal`
     fails with `PRIVATE_ADAPTER reported_facts=0x3 expected=0x7` and `ASSERTION FAILED: the native
     adapter did not report its identity and memory facts` (CTest exit 8, 1 of 7 failed), while the
     public `rhi.q1a.clear_readback` **passes** — exactly the portability split the review asked for.
     Restoring the line, touching the source and rebuilding returns 7/7.
  2. The control exposed two real defects before it could pass:
     - `SubmitAndWait` assigned `*ok = Require(...)` instead of accumulating, so an earlier assertion in
       a private callback was silently cleared and the test still reported `pass=2`. Fixed to `&=`; the
       mutation now fails as shown.
     - The mapping validator's block scanner only matched `enum class X {`, so the new
       `enum class AdapterFact : uint32_t` was invisible and a declaration could have gone unmapped
       unnoticed. The parser now accepts an optional underlying type, and a fixture scoped enum plus a
       `scoped-enum-detection` mutant protect it.
  3. A restored file kept its older timestamp and was not rebuilt, so a stale binary briefly reported
     green. The recorded runs are from a rebuild after touching the changed source.

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| Q1a suite after the header change | RUN | PASS | 7/7, hardware and software adapters | `build-rhi-t089/ctest-junit.xml` |
| Native `reportedFacts` | RUN | PASS | `0x7` for both adapters, asserted only under `tests/renderer/d3d12/` | `build/priv2.log` |
| Portable invariant | RUN | PASS | undefined bits rejected; a set bit implies a non-zero id | `rhi_clear_test.cpp` |
| Mutation `0x3` (private assertion) | RUN | PASS (control) | `state_journal` fails, public clear passes; restored run 7/7 | `build-rhi-t089.mut3-ctest.log` |
| Mapping completeness | RUN | PASS | 57/57 declarations classified | `validate-webgpu-mapping.py` |
| Mapping mutation suite | RUN | PASS | 14 document mutations, 14 validator mutants | validator suite output |
| Lexical header gate | RUN | PASS | 1 header + 3 public test files, zero prohibited hits | `validate-rhi-contract.py` |
| Out-of-range `adapterIndex` still `Unsupported` | RUN | PASS | index 9999 case passes (native backends may honour in-range indices) | CTest |
| Contract/header contradiction | RUN | PASS | gate rule now names the availability-signal exception; §Optional adapter facts defines it | `contracts/rhi-foundation.md` |
| Browser conformance spike | NOT RUN | — | unchanged subject; stopped by owner after PR #37 | `evidence-q7a.md` |
| Emscripten encoder leg | NOT RUN | — | T088 remains open; no toolchain authorised | `webgpu-mapping.md` §NOT RUN |

## Independent review

- **Independent reviewer**: a non-author agent reviewer with no authorship of the changed files;
  `review-t089.md` holds both passes.
- **Review scope and commands**: it reran the build, CTest, both validators and their self-tests, the
  header gate, the whitespace check and the browser spike; it reproduced the `0x3` mutation and a
  browser-shaped `reportedFacts = 0` run, and it judged the capability-signal option against moving the
  fields to `ext/`.
- **Review decision and unresolved items**: **accept with limitations**, no blocking or major finding
  left after the second pass. Its three minor findings were fixed: the evidence no longer calls the
  index-9999 case a rule about nonzero indices, G4 now separates an enumerating native backend from a
  backend that must return `Unsupported`, and the contract states that a set id bit implies a non-zero
  value. The remaining limitation is material: the `reportedFacts = 0` path is proven by construction,
  the validator and a browser-shaped run, **not** by an executing WebGPU backend, because none exists.
