# Evidence template: renderer stage or scoped check

**Template only — not a test result or review.** Copy into the stage's authorized evidence record; replace every placeholder, or explicitly record why it is unavailable. Never include game data, proprietary SDK binaries, secrets or machine-specific absolute paths in tracked records. Retain permitted artifacts in an ignored worktree-local directory or other authorized storage, with a portable relative reference and access conditions.

## Identity and scope

- **Stage and claim**: `<Q-stage, issue, exact behavior and intended claim>`
- **Source identity**: `<repository revision, branch, dirty paths, submodule SHA/status, patch and executable identities/hashes>`
- **Data identity and rights**: `<synthetic fixture/version/hash, or legal local data identity/hash and provenance; permitted use, modification, translation and distribution; missing permissions>`
- **Scene and inputs**: `<scene/view/camera or fixture, dimensions, seed, commands/inputs and relevant asset exclusions; N/A with reason if no scene>`
- **Exclusions and limitations**: `<unsupported cases, excluded regions/data, non-goals and reasons, or none>`

## Environment and configuration

- **Toolchain and configuration**: `<OS/architecture, compiler and version, build type/options, SDK/tool versions, enabled features, relevant environment and discovery method>`
- **Adapter and driver**: `<backend, named hardware/software adapter, driver/version, device and selection; N/A with reason if no device>`
- **Capability and fallback**: `<requested, advertised, enabled and actually exercised capabilities/limits; explicit fallback selected and reason, or none>`
- **Shader provenance and build**: `<source/permission identity, compiler/validator versions, target/profile, entry points, defines/includes, layouts, stages and artifact identities; N/A with reason>`

## Procedure and retained artifacts

- **Predeclared tolerances**: `<numeric per-check thresholds, hash rule/expected values, measurement units, exclusions and when fixed before results; N/A with reason>`
- **Commands and exits**: `<exact ordered commands, working-directory context, start/end or run identity and exit status for each; no secret values>`
- **Retained logs and artifacts**: `<portable references to full stdout/stderr, verbose/JUnit test log, captures and hashes; retention/access; missing artifacts and reason>`
- **Negative controls**: `<mutation/control, expected failure, actual result and restored run; N/A with reason>`

## Per-check results

Use one row per intended check and per adapter/diagnostic. `Execution` is `RUN`, `SKIPPED` or `NOT RUN`; `Outcome` is `PASS`/`FAIL` only for an actually RUN check, or `—` otherwise. Record a specific reason for every SKIPPED/NOT RUN case. An exit 0 or an advertised feature alone is not an executed test. `NOT APPLICABLE` is a reason for a genuinely inapplicable control, not a pass.

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| `<check, adapter or diagnostic>` | `<RUN / SKIPPED / NOT RUN>` | `<PASS / FAIL / —>` | `<observation or reason>` | `<relative reference or unavailable reason>` |

Record functional execution, hardware and software adapters, debug-layer enablement and GPU-based validation as separate rows when relevant. Full Q1 requires debug layer and GPU-based validation to actually execute with zero unwaived errors; a skipped check does not satisfy it. Record individual messages and waivers and their rationale; no implicit waiver.

## Independent review

- **Independent reviewer**: `<identity and independence from author; NOT RUN if unassigned>`
- **Review scope and commands**: `<revision/diff reviewed, matrix/rights/tolerance decisions, commands rerun and exits, retained review reference>`
- **Review decision and unresolved items**: `<accept / changes requested / NOT RUN; specific findings, remaining blockers and owner; do not equate documentation validation with technical approval>`
