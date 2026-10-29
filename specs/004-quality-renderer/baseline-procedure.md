# Q0 baseline procedure (T006)

Status: procedure, not evidence. Evidence is in [evidence-q0.md](./evidence-q0.md).
Scope: issue #21 / spec 004 Q0 tasks T006–T011. This procedure applies only to the existing
OpenGL renderer on Windows x64. It does not establish classic parity, modern-renderer
performance, or a web build.

## 0. The one rule

A historical `smoke*`, `run*`, `showcase-*`, `%TEMP%\d3*` or `docs/media` artifact is **not** Q0
evidence. Q0 evidence exists only when this procedure produced a new, named output tree in the
same run that recorded the executable, data, source and command identities below.

## 1. Physical facts and ownership

| Item | Fact |
|---|---|
| Branch / worktree | `feat/21-q0-safe-baseline`, one issue = one branch = one worktree |
| Build tree | a **new** `build-q0-<date>` directory in this worktree; never the root `build/` cache and never another worktree's build |
| Target copy | `targets/win11-amd64/doom3`, git-ignored, created/patched by `scripts/new-target.ps1` and `scripts/apply-target-patches.ps1` |
| Pristine source | `doom3_orig/doom3` submodule; read-only, clean, never edited in place |
| Game data | an external prepared run directory containing `base/demo00.pk4`; read-only input, never committed |
| Outputs | `<OutDir>\<scene>\run1|run2`, `baseline-manifest.json`, `comparison.json`; all under a new directory and git-ignored |
| Entry point | `scripts/test-render-baseline.ps1` (ListScenes, Check, Capture, Compare, All) |
| Synthetic tests | `tests/baseline/test-render-baseline.ps1`; issue-compatible launcher `tests/renderer/test-render-baseline.ps1` |

## 2. Why the older scripts are not the Q0 entry point

They remain useful for their own specs, but using them as a Q0 baseline would mix identities and
side effects. The audit below is from the source in this branch.

| Script | Behaviour that disqualifies it as the Q0 baseline entry point | Citation |
|---|---|---|
| `run-win11-amd64.ps1` | Chooses the executable by newest mtime between the root build and the target build, so it cannot prove which build ran; `-Clean` recursively deletes the output tree with no path guard; the default output directory is shared; `alive=True` is a literal string, not a re-check. | `scripts/run-win11-amd64.ps1:39,45-51,87,130` |
| `smoke-win11-amd64.ps1` | Two-phase output, `-Clean` deletes the output tree, arguments are passed as an unquoted array (a path with spaces is mis-parsed), and it records no build, source, data or GPU identity. | `scripts/smoke-win11-amd64.ps1:55-70,59` |
| `verify-win11-amd64.ps1` | Asserts PE machine and size only; leaves `targets/win11-amd64/build/smoke-<guid>` behind; does not record which executable hash, build directory or data identity was verified. | `scripts/verify-win11-amd64.ps1:21-27,107-108` |
| `export-target-patches.ps1` | The default `-OutDir` is the tracked `targets/<Name>/patches` and it overwrites both files without refusal. | `scripts/export-target-patches.ps1:24,80-81` |
| `new-target.ps1` | `-Force` deletes the existing target copy recursively; the post-copy check compares file counts, not hashes. | `scripts/new-target.ps1:58,70-75` |
| `apply-target-patches.ps1` | Applies into the target copy in place and unconditionally deletes `$env:GIT_CEILING_DIRECTORIES` in the caller's session. | `scripts/apply-target-patches.ps1:59-62` |
| `capture-window.ps1` | `-ProcessName` defaults to any process named `DOOM3`; it brings the window topmost, steals foreground and does not restore it. Q0 must pass `-ProcessId`. | `scripts/capture-window.ps1:33-34,51,59-60` |

`scripts/test-render-baseline.ps1` closes these gaps: explicit `-Exe`, SHA-256 and PE machine in
every manifest; a new `-OutDir` that must be a strict child of `-OutRoot` (or under `%TEMP%` / a
`build*` segment); non-empty output refused; `-Clean` renames the old tree to
`<OutDir>.superseded-<UTC>` instead of deleting it; only the PID it started is stopped; and a
`SKIPPED` result is exit 10 or a per-scene `SKIPPED` row, never a PASS.

## 3. Preconditions

Run all commands from the worktree root. Stop if any check fails; do not "work around" it.

```powershell
git rev-parse --show-toplevel
git branch --show-current
git status --porcelain
git -C doom3_orig/doom3 status --porcelain
git submodule status
```

- Branch is `feat/21-q0-safe-baseline` (or the branch the issue assigned). A detached HEAD is a stop.
- `doom3_orig/doom3` is initialised, clean, and at the recorded SHA.
- No process whose image path equals the resolved `DOOM3.exe` is running. The Q0 script refuses
  with exit 7; `-StopExisting` is consent to stop only that path, and its use must be recorded.
- The data root exists and contains `base/*.pk4`. The script treats it as read-only; it writes
  only below `fs_savepath` inside the new run directory.
- The output root is new or disposable; it is never a tracked directory.

## 4. Fresh build

Use the Visual Studio 2022 CMake binary explicitly; do not rely on `cmake` being on `PATH`.

```powershell
$cmake = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake -S . -B build-q0-<date> -G 'Visual Studio 17 2022' -A x64 `
  -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DDOOM3_TARGET=win11-amd64 -DCMAKE_CONFIGURATION_TYPES=Release `
  *> build-q0-configure.log
$configureExit = $LASTEXITCODE
& $cmake --build build-q0-<date> --config Release -- -m *> build-q0-build.log
$buildExit = $LASTEXITCODE
Set-Content -Path build-q0-<date>\build-exit.txt -Value "configure=$configureExit build=$buildExit"
```

Record `configureExit`, `buildExit`, the last link lines, warning/error counts, and the binary
hashes. A second fresh build is a reproducibility check, not a replacement for the captured
binary: record both hashes and say which one was captured. PE timestamps can make the bytes
differ even when the source is identical. In the recorded run CMake printed
`Ignoring extra path from command line: ".0.26100.0"` but still selected SDK 10.0.26100.0;
record the warning rather than hiding it.

## 5. Identity record

Before capture, record all of the following in the evidence file:

| Identity | Command / source |
|---|---|
| Repository commit | `git rev-parse HEAD` |
| Worktree dirt | `git status --porcelain` (the Q0 change set must be named; uncommitted is not "clean") |
| Submodule commit | `git -C doom3_orig/doom3 rev-parse HEAD` |
| Submodule dirt | `git -C doom3_orig/doom3 status --porcelain` |
| Target patch identity | SHA-256 of `targets/win11-amd64/patches/all-changes.patch` and `.manifest.txt` |
| Executable | path, bytes, SHA-256, PE machine, FileVersion if present |
| Compiler / SDK | MSVC version and Windows SDK from `build-q0-configure.log` |
| Data | `base/*.pk4` names, bytes and SHA-256; loose shims and their hashes |
| Host | OS build, CPU, GPU/adapters, driver version, RAM |
| Renderer | engine log line (`using ARB renderSystem`) and any reported video-memory/quality line |

The executable identity is the **captured** binary, not merely the newest file in a build tree.

## 6. Capture

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-render-baseline.ps1 `
  -Mode Check -Exe <build-q0-date>\bin\Release\DOOM3.exe -DataDir <D3_DATA_DIR> `
  -OutDir <build-q0-date>\baseline\out -OutRoot <build-q0-date> `
  -Scenes corridor,open-view,close-character,moving-light-combat

powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-render-baseline.ps1 `
  -Mode All -CaptureMethod Engine -Exe <build-q0-date>\bin\Release\DOOM3.exe -DataDir <D3_DATA_DIR> `
  -OutDir <build-q0-date>\baseline\out -OutRoot <build-q0-date> `
  -TimeoutSec 180 -Repeat 2
```

Omit `-Scenes` to use all four category scenes. Use one `-Scenes` id per invocation when invoking
through `-File`; a comma-separated string is not split into an array by that host.

The engine path is the Q0 method: it sets `com_fixedTic 1`, runs the scene for 600 tics (720 for
combat), captures with the engine's `+screenshot` at that exact frame, waits 30 frames, and quits.
The script converts the uncompressed true-colour TGA (24- or 32-bit) to PNG and compares the two PNGs. It does not use
an OS window capture, so window focus, topmost state and desktop composition cannot change the
result. The `Window` method remains available for smoke work but is not the Q0 evidence path.

Exit 0 with `result=PASS` is only the script's verdict. The evidence file must still list every
scene, every run, every metric and every hash. A per-scene `SKIPPED` is not a captured category.
If a category is unavailable, mark it blocked and say so; do not substitute a different view.

The four category ids, drive commands and per-scene tolerances are the contract in
[reference-scenes.md](./reference-scenes.md). Only a run where the contract was frozen **before**
the capture counts; exploratory calibration must be disclosed. Do not edit the drive command or
tolerance after the recorded run.

Each drive ends with `+echo SCENE_READY_<id>`. The engine screenshot must also have at least 50
distinct colours and a centre-region mean luminance of at least 2.0; a black frame is rejected.
`baseline-manifest.json` records a reconstructed command line (it records the capture phase as `mode=Capture` and is not guaranteed to replay verbatim; use the §6 command), the per-scene ceilings, the tic counts,
the centre luminance and the screenshot hashes.

`Compare` writes `comparison.json` into an existing capture tree and refuses (exit 5) if a report
already exists. To compare again, use a new output tree or move the old report. For a standalone
`Compare`, pass `-DataDir` and `-Exe` as well so the data-root and executable-directory guards
still apply.

## 7. Output ownership and cleanup

- The script writes only below `OutDir`; the engine's `fs_savepath` is `<OutDir>\<scene>\run<n>\save`.
- `OutDir` is refused if it is the repository root, a drive root, the data root, the executable
  directory, **inside** either of those trees, a tracked source directory, or outside `OutRoot` /
  `%TEMP%` / a `build*` segment. The data/exe guards apply in `Compare` too when `-DataDir` / `-Exe`
  are supplied.
- An existing non-empty `OutDir` is refused with exit 5 **before** the executable or the 483 MB
  data hash is read. `-Clean` is allowed only for Capture/All; it renames the old tree after the
  inputs and scene data validate, and never deletes it. A `Check` run never writes or archives.
  `Compare` refuses to overwrite an existing `comparison.json`.
- The script stops only the PID it started. It never stops a process by image name. If it finds
  the exact executable already running, it refuses unless `-StopExisting` was passed; record that
  consent.
- Do not reuse a shared `targets/win11-amd64/build/run` or `showcase-*` directory. Those belong to
  older scripts and may contain another session's artifacts.
- The `Engine` path writes the TGA under `<run>\save\base\screenshots\` and the converted PNG at
  `<run>\screenshot.png`; it does not use `capture-window.ps1`, so it does not change window
  focus or topmost state. It runs `+quit` and records `processExit=0`; a timeout is stopped by the
  exact PID and reported as a failure.
- The legacy `Window` path still uses `capture-window.ps1`, which sets the window topmost and
  steals foreground. Do not use it for Q0 evidence; run one capture at a time if you do.

## 8. Synthetic tests (no game data)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\baseline\test-render-baseline.ps1
# issue-compatible path:
powershell -NoProfile -ExecutionPolicy Bypass -File tests\renderer\test-render-baseline.ps1
```

The suite has 26 cases (`SUMMARY pass=26 fail=0` in the recorded run) and covers: valid Check, missing exe, missing/empty data, non-empty output refusal with an
untouched sentinel, refusal-before-exe-check, Capture refusal and fake-PE rejection, unsafe output
path, unknown scene, missing map entry, exe/data hash mismatch, identical/different/missing/
forbidden comparison inputs, Compare overwrite refusal, Compare inside the data tree, and two
deliberate mutations of the refusal logic. A mutation is "caught" when the mutated copy deviates
from the safe result (exit 5 with the sentinel intact); each mutant gets a fresh fixture, and the
test reports whether the sentinel survived. The suite does not start a real engine, so process
start/stop and the `-Clean` archive with a real PE are not behavior-tested; that remains a gap.

## 9. Evidence checklist

- [ ] Fresh configure + build exit 0, log paths and warning/error counts recorded.
- [ ] Executable SHA-256, bytes, PE machine and the build directory that produced it recorded.
- [ ] Repo commit, worktree dirt, submodule SHA/dirt, patch hashes recorded.
- [ ] Data files and hashes recorded; no game data committed.
- [ ] All four category ids listed; each is captured twice or explicitly SKIPPED with a reason.
- [ ] If every category is SKIPPED, the evidence says Q0 is not established; a single frame is not
      a category capture.
- [ ] `SCENE_READY_<id>` present in both runs; centre luminance and distinct colours recorded.
- [ ] Per-scene tolerance and measured mean-abs / diff-percent recorded; manifest carries the limits.
- [ ] Q0 script/test/doc file hashes recorded, or the change set is committed.
- [ ] Synthetic suite exit 0 and mutation cases recorded.
- [ ] `git diff --check`, submodule clean, no tracked binary/log/screenshot.
- [ ] Independent review recorded; unresolved findings listed as unresolved, not waived.
- [ ] No absolute machine path in the tracked evidence file.
