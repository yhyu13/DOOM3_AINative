# Evidence: T084 patch-delivery negative controls

**Status**: executed 2026-11-01 (T084, issue #99). The apply-path controls existed before this task;
this file records the new exporter and verifier controls and the mutation evidence that each control
can fail.

## Identity and scope

- **Stage and claim**: T084 — a reconstruction that contains an added (`A`) or deleted (`D`) row must
  not report PASS, a partial `M`-row match must not be counted as verified, and a patch set that
  disagrees with the manifest must fail closed. Scope is the three patch-delivery scripts plus the
  control script; no engine, submodule, target copy or GPU run is involved.
- **Delivery decision**: added/deleted target-copy delivery is **not** implemented. Engine files that
  do not exist in the pristine tree are delivered only through the reviewed fork plus submodule bump;
  see [`engine-integration.md`](./engine-integration.md) §3.3. The scripts enforce the boundary:
  the exporter exits 1 when an `A`/`D` row exists, the applier exits 1, and the verifier reports
  `SC-006a`/`SC-006b` FAIL.
- **Source identity**: superproject `b3d9b8a81bebae846acc30e48a3423474aff913c` before this PR's edits;
  submodule `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`, clean. The control suite never reads a real
  target copy, game data or the pristine submodule: it creates `targets/zz-patchtest-*` and
  `targets/zz-patchsrc-*` synthetic trees and removes them in `finally`.

## Environment and configuration

- Windows x64, Windows PowerShell 5.1, git 2.25.0.windows.1. No adapter, driver or shader.

## Procedure

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-target-patches.ps1
```

Exit 0; summary `TEST target-patches: 33 passed, 0 failed`.

The synthetic setup is: pristine `neo/file_a.cpp` = `alpha\n`, `neo/file_b.cpp` = `bravo\n`; target
`file_a.cpp` = `alpha-patched\n`, `file_b.cpp` = `bravo\n`. The exporter is invoked with
`-SourceDir <synthetic pristine>` so it never touches `doom3_orig/doom3`.

## Controls and results

| # | Control (assertion) | Script under test | Expected | Observed | Outcome |
|---|---|---|---|---|---|
| 3b | deleted (`D`) row, with and without `-AllowIncomplete` | apply-target-patches | exit 1 | exit 1 twice | PASS |
| 8 | modified-only export | export-target-patches | exit 0, M-only manifest, non-empty patch | exit 0, 1 M row | PASS |
| 9 | export with an added file | export-target-patches | exit 1, `A` row recorded | exit 1, 1 A row | PASS |
| 10 | export with a deleted file | export-target-patches | exit 1, `D` row recorded | exit 1, 1 D row | PASS |
| 11 | modified-only reconstruction | verify-win11-amd64 | exit 0, `M rows verified 1/1` | exit 0, 1/1 | PASS |
| 12 | verifier with an added file | verify-win11-amd64 | exit 1 | exit 1 (`SC-006a`/`SC-006b` FAIL) | PASS |
| 13 | verifier with a deleted file | verify-win11-amd64 | exit 1 | exit 1 (`SC-006a`/`SC-006b` FAIL) | PASS |
| 13b | supplied manifest with an `A` row / a `D` row (`-PatchDir`, exporter not involved) | verify-win11-amd64 | exit 1, `added/deleted row` | exit 1 both | PASS |
| 14 | partial `M` match, 1 of 2 rows (`-PatchDir`) | verify-win11-amd64 | exit 1, `M rows verified 1/2` | exit 1, 1/2 | PASS |
| 15 | patch disagrees with manifest hash (`-PatchDir`) | verify-win11-amd64 | exit 1, `M rows verified 0/1` | exit 1, 0/1 | PASS |
| 16 | manifest with no patch file (`-PatchDir`) | verify-win11-amd64 | exit 1, `git apply exit -1` | exit 1, -1 | PASS |

## Mutation evidence (fails before the fix, passes after)

Each mutation was applied to the fixed script, the same suite was run, then the mutation was reverted
and the suite rerun. The mutation lines are recorded verbatim.

### M1 — exporter reports an incomplete export as success (pre-fix behavior)

```diff
-    exit 1
+    exit 0 # MUTATION-T084: pre-fix behavior; restored immediately
```

`scripts\test-target-patches.ps1` → exit 1, `29 passed, 4 failed`:

```text
FAIL export with added file refuses PASS: expected exit 1, got 0
FAIL export with deleted file refuses PASS: expected exit 1, got 0
FAIL verify reports added/deleted incompleteness
FAIL verify reports deleted incompleteness
```

Restored; final run exit 0, `33 passed, 0 failed`. The two exporter exit-code controls are the direct proof that an `A`/`D` export cannot
report success; the two verifier reason assertions flip because the verifier then reports the manifest-row reason
instead of the export-exit reason (it still refuses the manifest independently).

### M2 — verifier's own `A`/`D` guard disabled

```diff
-} elseif ($nonM.Count -gt 0) {
+} elseif ($false -and ($nonM.Count -gt 0)) { # MUTATION-T084: nonM guard disabled; restored immediately
```

Exit 1, `29 passed, 4 failed`:

```text
FAIL verify refuses an A row in a supplied manifest: expected exit 1, got 0
FAIL verify reports the supplied A row
FAIL verify refuses a D row in a supplied manifest: expected exit 1, got 0
FAIL verify reports the supplied D row
```

This control uses `-PatchDir`, so it does not depend on the exporter's exit code. Restored; 33/0.

### M3 — partial `M` match counted as verified

```diff
-        $ok = ($applyCode -eq 0) -and ($mismatch -eq 0) -and ($matched -eq $mRows.Count)
+        $ok = ($applyCode -eq 0) -and ($matched -gt 0) # MUTATION-T084: partial match counted as verified; restored immediately
```

Exit 1, `32 passed, 1 failed`:

```text
FAIL verify refuses partial M-row match: expected exit 1, got 0
```

Restored; 33/0.

### M4 — manifest hashes ignored while `git apply` succeeds

```diff
-        $ok = ($applyCode -eq 0) -and ($mismatch -eq 0) -and ($matched -eq $mRows.Count)
+        $ok = ($applyCode -eq 0) # MUTATION-T084: manifest hashes ignored; restored immediately
```

Exit 1, `31 passed, 2 failed`:

```text
FAIL verify refuses partial M-row match: expected exit 1, got 0
FAIL verify fails patch/manifest disagreement: expected exit 1, got 0
```

Restored; 33/0. Control 16 still failed the mutated verifier (`git apply exit -1`), which shows the
missing-patch guard does not depend on the hash loop.

## Per-check result

| Check | Execution | Outcome | Reason or observed result | Reference |
|---|---|---|---|---|
| Control suite as fixed | RUN | PASS | exit 0, 33/33 | command above |
| Control suite under M1 | RUN | FAIL (expected) | 29/33; exporter controls red | mutation log |
| Control suite under M2 | RUN | FAIL (expected) | 29/33; verifier A/D controls red | mutation log |
| Control suite under M3 | RUN | FAIL (expected) | 32/33; partial control red | mutation log |
| Control suite under M4 | RUN | FAIL (expected) | 31/33; partial + disagreement controls red | mutation log |
| Post-restore rerun | RUN | PASS | exit 0, 33/33; no `MUTATION-T084` marker remains | `Select-String` 0 hits |

## Limitations

- The verifier's `-PatchDir`/`-Sc006Only` pair is a control-test seam. It is refused without
  `-Sc006Only`, so a stage run still exports from the target copy; a synthetic `SC-006a` PASS is
  labelled "control patch set", not an export result.
- `SC-006b` remains self-consistency: the manifest and the expected hashes are regenerated from the
  same target copy. These controls prove the fail-closed shape, not canonical-patch provenance
  (`engine-integration.md` §1.5 and §3.3).
- No control covers a target copy edited between export and apply; that is outside the manifest.

## Independent review

- **Independent reviewer**: NOT RUN. Authored by the implementer; the PR reviewer must rerun
  `scripts\test-target-patches.ps1` and the listed mutations or spot-check them.
