# Evidence: T077 tracked-path and provenance audit

**Status**: audit executed 2026-11-01 (T077, issue #99). This file records a scan, not a review or a
"repository is clean" claim. Findings that the scan cannot clear are listed with their owner.

## Identity and scope

- **Stage and claim**: T077 鈥?audit every tracked path and its provenance against
  [`evidence-template.md`](./evidence-template.md), excluding game data, derived packs, binaries,
  SDKs, secrets and machine-specific absolute paths. The claim is the path table and the findings
  below, not that every finding is already fixed.
- **Source identity**: superproject `b3d9b8a81bebae846acc30e48a3423474aff913c` (`origin/main`; the
  `spec-004-t077-t084` worktree was equal to it before this PR's edits). Path counts are taken from
  that revision with `git ls-tree`, so this PR's own files are deliberately excluded and a rerun
  gives the same numbers. Submodule `doom3_orig/doom3` at
  `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`, `git status --porcelain` empty.
- **Data identity and rights**: no `.pk4`/`.pk3`/`.pak`, archive, executable, object/library,
  private-key or secret file is tracked (counts below). Project-owned images carry a `LICENSE` and/or
  provenance record: `content/hero/**` is CC0-1.0 with a generator and `provenance.json`;
  `tests/content/fixtures/golden-*.png` come from the deterministic
  `tests/content/fixtures/make-golden.py`; spec-004 evidence captures are documented as synthetic
  in `evidence-q4.md` / `evidence-q7a.md`. Two provenance findings remain (F1, F2).
- **Exclusions and limitations**: the scan classifies by path/extension and pattern; it does not
  adjudicate image rights or copyright. A NUL-byte check covers opaque non-image files. Whether a
  recorded third-party right is sufficient is a legal review, not this audit.

## Environment and configuration

- **Toolchain**: Windows x64, Windows PowerShell 5.1, git 2.25.0.windows.1. No compiler, GPU,
  adapter, driver or shader is involved. Run from the repository root.
- **Capability and fallback**: `git ls-tree` is used instead of `git ls-files` so the base revision is
  explicit; no fallback.

## Procedure and retained artifacts

The complete scan is reproducible with the following block. It exits 0; the scanned revision is the
base SHA above. The output was captured to a worktree-external temp log; the log is not tracked.

```powershell
$base = 'b3d9b8a81bebae846acc30e48a3423474aff913c'
$tracked = git ls-tree -r --name-only $base
function Get-PathClass([string]$p) {
    if ($p -eq 'doom3_orig/doom3') { return 'engine submodule gitlink' }
    if ($p -like 'doom3_orig/doc/*') { return 'engine analysis docs' }
    if ($p -like '.claude/skills/*') { return 'vendored agent skills' }
    if ($p -match '^specs/.*/evidence/.*\.(png|jpg)$') { return 'evidence images' }
    if ($p -like 'specs/*') { return 'spec documents' }
    if ($p -like 'tests/content/fixtures/*') { return 'test fixtures' }
    if ($p -like 'content/*') { return 'generated content assets' }
    if ($p -like 'tests/*') { return 'tests' }
    if ($p -like 'src/*') { return 'superproject source' }
    if ($p -like 'shaders/*') { return 'shader sources and manifests' }
    if ($p -like 'web/*') { return 'web profile source' }
    if ($p -like 'scripts/*') { return 'workflow automation' }
    if ($p -like '.specify/*') { return 'spec-kit infrastructure' }
    if ($p -like '.kilo/*') { return 'kilo commands and config' }
    if ($p -like 'targets/*') { return 'target build files and tracked patches' }
    if ($p -like 'showcase/*') { return 'showcase assets and code' }
    if ($p -like 'docs/media/*') { return 'engine-rendered screenshots' }
    if ($p -like 'docs/*') { return 'project docs' }
    return 'repository metadata'
}
$rows = $tracked | ForEach-Object { [pscustomobject]@{ Path = $_; Class = Get-PathClass $_ } }
Write-Host ("TRACKED_TOTAL={0}" -f @($tracked).Count)
$rows | Group-Object Class | Sort-Object Count -Descending |
    ForEach-Object { Write-Host ("CLASS {0,4}  {1}" -f $_.Count, $_.Name) }
Write-Host ("CLASS_SUM={0}" -f (@($rows | Group-Object Class | Measure-Object -Property Count -Sum).Sum))
$game = @($tracked | Where-Object { $_ -match '\.(pk4|pk3|pak|dds|tga|roq|md5mesh|md5anim|md5model|lwo|ase)$' })
$packs = @($tracked | Where-Object { $_ -match '\.(zip|7z|rar|tar|gz|bz2|xz)$' })
$bin = @($tracked | Where-Object { $_ -match '\.(exe|dll|lib|obj|pdb|so|dylib|a|msi|iso|wasm|class|jar|snk|pfx|p12)$' })
$sdk = @($tracked | Where-Object { $_ -match '(?i)(^|/)(sdk|third[_-]?party|vendor|external)(/|$)' })
$keyname = @($tracked | Where-Object { $_ -match '(?i)(credential|secret|token|apikey|api-key|\.env($|\.)|\.pem$|\.pfx$|\.p12$|\.key$|id_rsa|\.crt$|\.keystore$)' })
Write-Host ("FORBIDDEN game_data={0} derived_packs={1} binaries={2} sdk_dirs={3} secret_filenames={4}" -f $game.Count, $packs.Count, $bin.Count, $sdk.Count, $keyname.Count)
$text = $tracked | Where-Object { $_ -match '\.(md|txt|ps1|py|cpp|h|hlsl|hlsli|json|ya?ml|cmake|mtr|sh|cmd|js|mjs|ts|html|css|patch|gitignore|gitattributes|gitmodules)$' -or [IO.Path]::GetFileName($_) -in @('CMakeLists.txt', 'LICENSE') }
$absPattern = 'C:\\Users\\[^\\\s]+|C:\\Git-repo[^\\\s]*|D:\\[^\\\s]+|/home/[A-Za-z0-9._-]+|/Users/[A-Za-z0-9._-]+'
$abs = foreach ($f in $text) { if (Test-Path -LiteralPath $f -PathType Leaf) { Select-String -LiteralPath $f -Pattern $absPattern -CaseSensitive -ErrorAction SilentlyContinue } }
Write-Host ("ABSOLUTE_PATH_CANDIDATES={0}" -f @($abs).Count)
$abs | ForEach-Object { Write-Host ("  {0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim()) }
$secretPatterns = [ordered]@{
    'private-key'     = '-----BEGIN (RSA |EC |OPENSSH |PGP )?PRIVATE KEY-----'
    'aws-access-key'  = 'AKIA[0-9A-Z]{16}'
    'github-token'    = 'gh[pousr]_[A-Za-z0-9]{36,}'
    'slack-token'     = 'xox[baprs]-[A-Za-z0-9-]{10,}'
    'openai-key'      = 'sk-[A-Za-z0-9_-]{20,}'
    'google-api-key'  = 'AIza[0-9A-Za-z_-]{35}'
    'password-assign' = '(?i)(password|passwd|pwd)\s*[:=]\s*["''][^"'']{4,}'
}
foreach ($k in $secretPatterns.Keys) {
    $h = foreach ($f in $text) { if (Test-Path -LiteralPath $f -PathType Leaf) { Select-String -LiteralPath $f -Pattern $secretPatterns[$k] -CaseSensitive -ErrorAction SilentlyContinue } }
    Write-Host ("SECRET {0}={1}" -f $k, @($h).Count)
}
Write-Host ('SUBMODULE_STATUS=' + (@(git submodule status) -join ' | '))
Write-Host ('PRISTINE_PORCELAIN_LINES=' + @(git -C doom3_orig/doom3 status --porcelain | Where-Object { $_ -and $_.Trim() }).Count)
```

Additional opaque-file check (any tracked file containing a NUL byte that is not a permitted image):

```powershell
git ls-tree -r --name-only b3d9b8a81bebae846acc30e48a3423474aff913c | ForEach-Object {
    if (Test-Path -LiteralPath $_ -PathType Leaf) {
        $b = [IO.File]::ReadAllBytes($_)
        if ($b -contains 0 -and $_ -notmatch '\.(png|jpg)$') { "BINARY $_" }
    }
}
```

Both blocks exited 0 and printed no `BINARY` line.

## Path class 鈫?count 鈫?verdict

Counts are for the 610 tracked paths at the base revision; the class counts sum to 610.

| Class | Count | Verdict | Basis |
|---|---:|---|---|
| spec documents | 129 | **FAIL** | F1: three absolute paths in review records |
| generated content assets | 81 | PASS | `content/hero/**` is CC0-1.0 with `LICENSE` + `provenance.json` |
| superproject source | 76 | PASS | C++/headers; no secrets |
| tests | 68 | PASS | Tests and scripts; no game data |
| vendored agent skills | 51 | PASS | Third-party notices recorded in `THIRD_PARTY_NOTICES.md` |
| workflow automation | 36 | PASS | PowerShell scripts; example paths only |
| showcase assets and code | 24 | PASS (recorded rights) | Derivative images and pending rights are recorded in `showcase/README.md` |
| evidence images | 24 | PASS | 22 spec-004 synthetic captures are documented in `evidence-q4.md`/`evidence-q7a.md`; the 2 in `specs/008` were not re-audited here |
| shader sources and manifests | 22 | PASS | Source + `provenance.json`; no binary SDK |
| spec-kit infrastructure | 21 | PASS | MIT, `THIRD_PARTY_NOTICES.md` |
| target build files and tracked patches | 14 | PASS | Two patch sets, manifests are M-only (72/66 rows, 0 A/D) |
| repository metadata | 13 | PASS | Root config/licence files |
| test fixtures | 11 | PASS | `make-golden.py` deterministic, no captured/retail image |
| kilo commands and config | 11 | PASS | Command prompts/setup script |
| engine analysis docs | 9 | PASS | Analysis text, no game data |
| web profile source | 9 | PASS | Source + tests |
| engine-rendered screenshots | 6 | **FAIL** | F2: data identity/rights not recorded |
| project docs | 4 | PASS | No secrets/absolute paths |
| engine submodule gitlink | 1 | PASS | Pinned `a9c49da鈥, clean |
| **Total** | **610** | 鈥?| 鈥?|

## Forbidden category 鈫?count 鈫?verdict

| Category | Count | Verdict | Evidence |
|---|---:|---|---|
| Game data (`.pk4`/`.pk3`/`.pak`/`.dds`/`.tga`/`.roq`/`.md5*`) | 0 | PASS | Extension scan; no such file |
| Derived packs/archives (`.zip`/`.7z`/`.rar`/`.tar`/`.gz`/`.xz`) | 0 | PASS | Extension scan; no such file |
| Binaries (`.exe`/`.dll`/`.lib`/`.obj`/`.pdb`/`.so`/`.a`/`.msi`/`.wasm`) | 0 | PASS | Extension scan + NUL check |
| Vendored SDKs (`sdk/`, `third_party/`, `vendor/`, `external/`) | 0 | PASS | Path scan; tools are host-provided, not tracked |
| Secret filenames (`.env`, `.pem`, `.pfx`, `.key`, `id_rsa`, 鈥? | 0 | PASS | Filename scan |
| Secret content (private key, AWS/GitHub/Slack/OpenAI/Google tokens, password assignment) | 0 | PASS | Case-sensitive scan; all seven patterns 0 |
| Machine-specific absolute paths | 3 | **FAIL** | F1 below |

The two case-insensitive hits in `showcase/index.html` (`AkIA鈥, `aIza鈥) are substrings of base64
audio inside one data URI; a case-sensitive rerun is 0. They are not keys.

## Findings

| Id | Severity | Location | Finding | Verdict / owner |
|---|---|---|---|---|
| F1 | MAJOR | `specs/004-quality-renderer/review-t088-encoder.md:6`, `review-t089.md:11,172` | Three author-specific absolute paths (`D:\git-repo-ai\鈥, `C:\Users\1\AppData\Local\Temp\鈥) in tracked records, contrary to the template. | **FAIL.** Not rewritten here: review records are independent evidence and the T077 auditor is not their author. Redact to `<worktree>` in a follow-up; owner: spec-004 reviewer. |
| F2 | MAJOR | `docs/media/01-鈥pg` 鈥?`06-underground-pipes.jpg` (6 files) | Engine-rendered DOOM 3 frames are tracked, but no tracked record states the data set, its rights or the capture command. The README only says "rendered by the x64 engine". | **FAIL.** Add a rights/provenance sidecar or replace with synthetic captures; owner: docs/media author. |
| F3 | INFO | `showcase/assets/hero-redacted.png`, `showcase/assets/aigc/page2-modern.jpg` | Derived third-party media with rights retained by id/Bethesda and licensing pending. | Recorded in `showcase/README.md`; accepted risk by the owner. Not a hidden violation, but not a clearance either. |
| F4 | INFO | `targets/*/patches/*` | Two tracked patch sets (72 and 66 rows) are M-only; no A/D rows, no game data. | PASS. They are source diffs, not derived data packs. |

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| T077-P1 path inventory | RUN | PASS | 610 tracked paths; class counts sum to 610 | command block above |
| T077-P2 forbidden extensions | RUN | PASS | game 0, packs 0, binaries 0, SDK dirs 0 | command output |
| T077-P3 opaque non-image binaries | RUN | PASS | no NUL-containing non-image file | second block |
| T077-P4 submodule pointer | RUN | PASS | 1 gitlink `a9c49da鈥, porcelain 0 lines | command output |
| T077-P5 secret content and filenames | RUN | PASS | all patterns 0; 2 base64 false positives | command output |
| T077-P6 machine-specific absolute paths | RUN | FAIL | 11 candidates: 3 real hits in 2 review records (F1); 2 help-text examples, 3 historical quotes in `specs/001`, 2 examples in `specs/010`, and 1 synthetic fixture in `tests/content/run-gates.ps1:251` are not machine state | command output |
| T077-P7 project-owned image provenance | RUN | PASS | CC0 licences, `provenance.json`, deterministic generator, synthetic-capture notes | `content/hero/**/LICENSE`, `tests/content/fixtures/make-golden.py` |
| T077-P8 `docs/media` rights/provenance | RUN | FAIL | six tracked captures have no data/rights record (F2) | `README.md:75`, no sidecar |
| T077-P9 showcase derivative media | RUN | PASS (recorded) | rights holder and pending status recorded | `showcase/README.md` 搂Licensing |
| T077-P10 tracked patch manifests | RUN | PASS | 72 M / 0 A / 0 D and 66 M / 0 A / 0 D | `targets/*/patches/*.manifest.txt` |

## Independent review

- **Independent reviewer**: NOT RUN. This audit was written by the T077/T084 implementer; the PR
  reviewer is the independent check. Re-running the block is the expected review command.

