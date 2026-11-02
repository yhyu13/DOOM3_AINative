# Q6/Q7 ledger reconciliation (2026-11-02)

Status: reconciliation of the Q6 (US7) rows T069/T070 and the Q7 (US8) rows T072–T075 against
merged work. This is **not** Q6 or Q7 acceptance, not a new independent review, and not a visual
claim. It changes `tasks.md`, this record and nothing else: no code, test, target CMake, Q2–Q5 file,
mapping or game data is touched.

## Source identity and method

- Branch `docs/q6-q7-ledger`, based on `main` at `c28fdf1` (the #128 merge); `origin/main` is the
  same commit.
- Submodule `doom3_orig/doom3` at `cd6ae660f4b8de2cd9b7624017e17c89124e8ccb`;
  `git -C doom3_orig/doom3 status --porcelain` is empty.
- PR #118 (`feat/q6-visual-captures`) is **MERGED**: reviewed head and PR head are both
  `8b19b5b6a3c0e54080e64e0ff0375937b818f85f`, merge commit
  `c63f57a96c451e215d2b494d51afeb55d36f6cc6`, base `b7c1a680` at review time.
- PR #119 (`feat/q7-t072-t075`) is **MERGED**: reviewed head and PR head are both
  `925f81c3deabedecf52b7f23d97ca838df764ce8`, merge commit
  `c4509cdef1375af631e8a3cc4d8185231cacb53c`, base `88152396` at review time.
- Both APPROVED comments carry reviewer session `ses_e782c579bffe020RqOqN1Qx0EK`, whose worktree is
  `review-pr-56-policy-f6387fb74156381b`. The Q6 authoring session was
  `ses_e65eb559bffe7ptVfqobauG9Kt` (`feat-q6-visual-captures-0ff9c1c48561a139`) and the Q7
  authoring session was `ses_e65dad6f3ffe2SBIN5c0QfKWmr`
  (`feat-q7-t072-t075-c2443dfcbc9d5a2e`). The recorded sessions and worktrees differ, which is the
  process evidence for independence; this is not cryptographic identity and is not claimed as more.
- `specs/004-quality-renderer/review-q6.md` and `review-q7.md` do not exist (`Test-Path` is `False`
  for both). The merged verdicts are PR comments, not those artifacts.

### Commands run here

Logs are in the git-ignored `build-q6q7-ledger*` paths.

| Command | Exit | Observed |
|---|---|---|
| `cmake -S tests/renderer -B build-q6q7-ledger -G "Visual Studio 17 2022" -A x64` | 0 | configured with MSVC 14.44.35207 |
| `cmake --build build-q6q7-ledger --config Release --target q7_portable_profile_test -- -m` | 0 | `q7_profile.lib` and `q7_portable_profile_test.exe` built |
| `build-q6q7-ledger\Release\q7_portable_profile_test.exe all` | 0 | `SUMMARY portable_profile checks=537 failures=0` |
| `ctest --test-dir build-q6q7-ledger -C Release --no-tests=error --output-on-failure -R portable_profile` | 0 | 6/6 passed, 0 failed |
| `node --test tests/webgpu/profile.test.mjs tests/webgpu/profile-source-guard.test.mjs` | 0 | 25 tests, 25 pass, 0 fail |
| `node tests/webgpu/run-profile.mjs --browser <host chrome.exe> --repeat 2 --out <scratch> --evidence <scratch>` | 0 | attempt 1 and 2: 11 PASS / 0 FAIL / 4 NOT RUN (15 checks); repeatable; pinned major 156 (Chrome `156.0.8078.76`) |
| SHA-256 of the 13 committed `evidence/t069/*.png` against `evidence-q6.md` §4.5.6 | — | 13/13 match |
| SHA-256 of the 7 committed Q7 blobs named in `evidence-q7.md` against the recorded values | — | 7/7 match |
| Scan for `*.pk4` / `*.pk3` / `*.pak` under the worktree; `Test-Path _data` | — | 0 files; `_data` absent |
| `Test-Path specs/004-quality-renderer/review-q6.md` / `review-q7.md` | — | `False` / `False` |

The `tests/dxr-capture` DXR tool was **not** rerun here. Its committed images were hashed against
the recorded table, and the merged PR's independent review reran the tool and reproduced all 13 PNGs
byte-for-byte; this record does not add a second capture run.

## Corrected ledger

Status vocabulary: **DONE** = every deliverable and stated condition in the row is evidenced;
**PARTIAL** = at least one stated deliverable is delivered and at least one is absent or unreviewed;
**BLOCKED** = the remaining work cannot proceed with the data, hardware, branch or review available
here; **NOT RUN** = the check did not execute.

| Task | Status | Evidenced now | Unmet condition / exact reason |
|---|---|---|---|
| T069 | **PARTIAL** | 13 committed PNGs plus `evidence/t069/report.json` are present; all 13 SHA-256 values match `evidence-q6.md` §4.5.6. The captures come from a real DXR device (`Intel(R) Graphics`, tier `1_1`, SM 6.6, UMD `32.0.101.8331`), with convergence (`mae` 0.001309, `p99` 0.028725, scaling 1.5271), flat-field denoise 0.0810, reference timing (median 5.3880 ms, p95 6.6128 ms at 640x360, 1 spp/pass, not a gate) and deterministic reruns. PR #118 merged at reviewed head `8b19b5b` with an independent APPROVED comment. | The fixtures are synthetic: they are not the Q4 content cells, not PT-S1..S5, and not a Q6 engine frame. PT-S5 is `NOT RUN`; input latency, VRAM, debug-layer errors, edge bleed and temporal history are `NOT RUN`; the 4096-spp reference is the same estimator; the capture host is not the RX 9070 XT named in the brief. The row stays unticked. |
| T070 | **BLOCKED / NOT RUN** | An independent APPROVED comment exists at PR #118's reviewed head `8b19b5b` (session `ses_e782c579bffe020RqOqN1Qx0EK`), with the rerun command, the 13 reproduced hashes and the host/limits checked. | That verdict is capture-scope: it explicitly records `T069 stays PARTIAL`, `T070 NOT RUN`, no PT-S1..S5 identity and no playability claim. `review-q6.md` does not exist. There is no Q6 engine frame or playable showcase to review against T063 and the art direction, so the Q6 review the row requires cannot be written. The implementer cannot author it. |
| T072 | **DONE** | The backend-neutral fixture and implementation exist at `tests/renderer/portable_profile_test.cpp` and `src/renderer/rhi/capability_profile.cpp`. The rerun here exits 0 with `checks=537 failures=0` and CTest `6/6`. The fixture encodes 14 required limits, 4 fallback ids and 14 excluded classes independently of the implementation, with negative controls. | — |
| T073 | **DONE for `webgpu-browser` only** | The chosen target is explicit (`targetId=webgpu-browser`) and its adapter/host tests live under `web/profile/**` and `tests/webgpu/**`, which `portable-profile-contract.md` §5 assigns to that profile. The rerun here passes 25/25 Node tests and the pinned Chrome 156 harness records 11 PASS / 0 FAIL / 4 NOT RUN twice, repeatable, with the live 18-row capability table (`DEGRADED`, 4 fallbacks, 0 unsupported). | No other target is claimed. The C++ `src/renderer/profiles/profile_backend.cpp` route named in the row was frozen for this slice and remains unbuilt; no Windows pass is used to infer this target. The 4 NOT RUN rows (pointer lock, browser audio gesture, device loss, real scene) are recorded, not passed. |
| T074 | **BLOCKED / NOT RUN** | `evidence-q7.md` and `evidence/q7-profile.json` record `R1-real-scene` as `NOT RUN` with the exact reason, and the harness fails a claimed PASS. PR #119 merged at reviewed head `925f81c`. | `no legal DOOM 3 game data is present in this repository; retail pk4/pk3 assets are EULA-bound and were not supplied for this slice`. The scan here finds 0 `.pk4`/`.pk3`/`.pak` files and no `_data/` mount; issue #105 remains open. The procedural fixture is not a real-scene pass. |
| T075 | **BLOCKED / NOT RUN** | An independent APPROVED comment exists at PR #119's reviewed head `925f81c` (session `ses_e782c579bffe020RqOqN1Qx0EK`). It checks the capability table (`DEGRADED`, 18 rows, 4 fallbacks, 0 unsupported), the 25 Node tests, the 6/6 T072 fixture and the twice-repeated 11/0/4 harness result. | It is a PR merge-gate verdict, not the row's artifact: `review-q7.md` does not exist. The verdict itself leaves T074 and T075 `NOT RUN`, and the unsupported/real-scene cases beyond the procedural host slice were not executed because T074 is blocked. The author cannot write this verdict. |

Only T072 and T073 are ticked, and they were already ticked before this reconciliation. No Q6 or Q7
row is added to the ticked set by this pass.

## Corrections to the previous annotations

- **T070's previous note is stale and wrong on two points.** It said "no independent reviewer has
  recorded a verdict, and the review's own inputs (T069 captures) do not exist." A verdict exists
  (PR #118, `8b19b5b`) and the captures exist (13 PNGs, hashes 13/13). The row stays unticked for a
  different reason: the verdict covers synthetic captures only and explicitly declines Q6 acceptance,
  and `review-q6.md` was never written.
- **T075's previous note is stale.** It said review was "requested on the PR"; PR #119 merged at the
  reviewed SHA with an APPROVED comment. The row stays unticked because the approval is a PR-scope
  merge gate, not `review-q7.md`, and the reviewer itself left T074/T075 `NOT RUN`.
- **T069's note gains the merge and byte-verification facts.** Its status is unchanged: PARTIAL,
  unticked, because the captures are synthetic fixtures rather than PT-S1..S5 engine frames.
- **T074's note is correct.** It gains the merge identity and the zero-pak scan; the reason is
  unchanged.
- **`evidence-q7.md` §"Independent review" is now stale.** It still says "NOT RUN" and "reviewer not
  yet assigned." The merged PR comment is the later fact. This reconciliation leaves the author
  evidence file untouched and records the PR-scope verdict here.
- **`evidence-q6.md` headline line 13 is stale in one clause.** It says there is "still no Q6 ...
  capture, image, frame time"; §4.5 and this pass record synthetic captures, images and reference
  timing. The statement remains true for a Q6 engine frame, PT-S1..S5, VRAM and input latency.
- **`path-tracing-rubric.md` line 7 and `docs/ROADMAP.md` line 205 are stale in the same way.** The
  rubric says "No Q6 capture ... exists" while its §7 records the synthetic fixtures; the roadmap
  lists "any capture, convergence record, frame time" as missing while §4.5 now has all three on
  synthetic content. Neither file is edited here; both must keep saying Q6 is PARTIAL and no
  PT-S1..S5 or playability claim is made.

## Remaining blockers

| # | Blocker | Rows affected | Unblock condition |
|---|---|---|---|
| B1 | No Q6 engine frame and no PT-S1..S5 content capture; the capture tool is synthetic and standalone | T069 (PT-S1..S5, input latency, VRAM, temporal history), T070 | Engine/fork wiring for a Q6 frame, then a named-content capture at a named adapter; this is outside a documentation pass |
| B2 | No `review-q6.md`; the only independent Q6 verdict is capture-scope | T070 | A non-author reviewer writes `review-q6.md` against T063 and the art direction, at a reviewed SHA, only once a Q6 engine frame exists |
| B3 | No legal DOOM 3 game data; issue #105 is open | T074, and T075's real-scene/unsupported-case coverage | An owned retail install or licensed source under untracked `_data/`, or the rows are permanently marked out of scope |
| B4 | No `review-q7.md`; the only independent Q7 verdict is a PR merge gate that itself leaves T075 `NOT RUN` | T075 | A non-author reviewer writes `review-q7.md` covering the claimed profile and every unsupported case, after B3 permits a real-scene run |
| B5 | The C++ `src/renderer/profiles/profile_backend.cpp` route is frozen/unbuilt for this slice | T073 (scope caveat, not a failure of the chosen browser profile) | Owner selects the route and a later slice implements it; the browser profile is already contract-assigned to `web/profile/**` |

## Non-claims

- Q6 and Q7 are **not accepted**. T069 is PARTIAL; T070, T074 and T075 are not done.
- The 13 PNGs are synthetic DXR fixtures, not Q4 content and not a Q6 engine frame. No PT-S1..S5
  identity, playable showcase, input-latency, VRAM or hybrid-comparison result is claimed.
- The browser harness proves host plumbing and a procedural scene only. It is not a real-scene pass,
  not a WebGPU `doom3::rhi::Device`, and not a browser support claim.
- The independent APPROVED comments on #118 and #119 are merge-gate evidence. They do not substitute
  for `review-q6.md` or `review-q7.md`, and neither verdict claims Q6/Q7 acceptance.
- No Q2, Q3, Q4, Q5, target or mapping file is changed by this pass.
