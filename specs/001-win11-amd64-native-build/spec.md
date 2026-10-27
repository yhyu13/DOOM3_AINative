# Feature Specification: Native Windows 11 x64 build of DOOM 3

**Feature Branch**: `001-win11-amd64-native-build` (worktree branch `feat/1-win11-amd64-native-build`)
**Created**: 2026-10-26
**Status**: Draft
**Issue**: #1
**Input**: "Adapt DOOM 3 to the current Win64 / Windows 11 system so it runs on this computer, in `targets/win11-amd64`."

## Problem

The pristine DOOM 3 GPL source (`doom3_orig/doom3`) only builds with Visual Studio 2010 for 32-bit
Windows, needs the DirectX June 2010 SDK, uses about 1,550 MSVC inline `__asm` blocks, and ships a
32-bit OpenAL. None of that exists on this machine (VS2022, Windows SDK 10.0.26100, x64). Without
a working native baseline, later work (web port, AI modding layer, headless agent API) has nothing
to compare against or test with.

## User Scenarios & Testing

### User Story 1 - Engine builds for x64 (Priority: P1)

A developer runs one documented command in a fresh worktree and gets an x64 engine executable and
game module built from the target copy, without VS2010 or the DirectX June 2010 SDK.

**Why this priority**: Everything else depends on it.
**Independent Test**: Run the build command; check exit code, PE machine type and file size.
**Acceptance Scenarios**:
1. **Given** a fresh worktree with the target copy created by `new-target.ps1`, **When** the build command runs, **Then** it exits 0 and produces an x64 executable larger than 1 MB.
2. **Given** the build output, **When** the PE header is read, **Then** the machine type is x64 (0x8664).
3. **Given** the pristine submodule, **When** the build finishes, **Then** `git status --porcelain` inside it is empty.

### User Story 2 - Engine starts and reaches the menu with legal demo data (Priority: P1)

With legally obtained DOOM 3 data in an untracked directory, the engine opens a window, initialises
the renderer, and shows the main menu on this PC.

**Why this priority**: Proves the port runs, not only links.
**Independent Test**: Smoke script launches the exe, waits, captures a screenshot and the log.
**Acceptance Scenarios**:
1. **Given** demo data in the data directory, **When** the engine starts, **Then** the log reports renderer init and the main menu GUI loads without a fatal error.
2. **Given** a running engine, **When** the smoke script captures the window, **Then** a 1280x720 screenshot shows a non-blank menu frame.
3. **Given** a console command to load a map, **When** executed, **Then** the map loads (if the data contains one) or the failure is logged with a clear reason.

### User Story 3 - Reproducible verification (Priority: P2)

A reviewer reruns one verification command and gets the same pass/fail evidence block used in the PR.

**Independent Test**: `scripts/verify-win11-amd64.ps1` exits 0 and prints the evidence lines.
**Acceptance Scenarios**:
1. **Given** a clean worktree, **When** the script runs, **Then** it reports each acceptance check as PASS/FAIL with exit codes, sizes and screenshot path.
2. **Given** missing data, **When** the script runs, **Then** it reports data-dependent checks as SKIPPED, not PASS.

### Edge Cases

- Retail/demo data missing or in a different layout: clear error, no crash loop.
- No DirectX SDK, no EAX/OpenAL, no network: engine must still start (these features may be disabled).
- Game module name/arch suffix hard-coded to x86: loading must still find the x64 module.
- Machine has only Intel integrated graphics with a modern driver: the legacy GL path must not hard-fail at start.
- Path containing spaces or non-ASCII characters.

## Requirements

### Functional Requirements

- **FR-001**: The build MUST use only tools present on this machine (VS2022 v143, Windows SDK) and MUST NOT require VS2010 or the DirectX June 2010 SDK.
- **FR-002**: The build MUST target x64 and MUST be driven from `targets/win11-amd64/doom3`; the pristine submodule MUST remain unmodified.
- **FR-003**: Code that cannot compile for x64 (MSVC inline assembly, 32-bit-only libraries) MUST have a documented portable fallback, or be disabled with a recorded reason.
- **FR-004**: Pointer-size assumptions that corrupt behaviour on x64 MUST be identified and fixed or recorded as open defects.
- **FR-005**: The engine MUST locate and load the game logic module on x64.
- **FR-006**: Game data MUST be kept outside git; the data location MUST be configurable by environment variable or script argument.
- **FR-007**: All target changes MUST be exportable as patches under `targets/win11-amd64/patches/` so they can be reviewed and landed upstream on the fork.
- **FR-008**: A smoke script MUST start the engine, capture a log and a screenshot, and exit with a meaningful code.
- **FR-009**: Windows-only editor tools (MFC) MAY be excluded from the build; the exclusion MUST be listed.

### Key Entities

- **Target copy**: `targets/win11-amd64/doom3`, git-ignored working tree derived from the pristine source.
- **Patch set**: ordered patches under `targets/win11-amd64/patches/`, the reviewable form of target changes.
- **Data directory**: untracked folder holding legally obtained DOOM 3 `.pk4` data.
- **Verification evidence**: log, PE report and screenshot produced by the verify script.

## Success Criteria

### Measurable Outcomes

- **SC-001**: Build command exits 0 and the executable's PE machine type is 0x8664.
- **SC-002**: Executable size is greater than 1,048,576 bytes.
- **SC-003**: With demo data present, the smoke script reaches the main menu within 120 seconds and saves a screenshot at 1280x720 that is not a single flat colour.
- **SC-004**: `git -C doom3_orig/doom3 status --porcelain` prints 0 lines after build and run.
- **SC-005**: `scripts/verify-win11-amd64.ps1` exits 0 on a clean worktree and prints PASS/SKIPPED per criterion.
- **SC-006**: Every change to the target copy is present as a patch file; applying the patches to a fresh copy reproduces the same build.

## Assumptions

- Retail data is not owned/installed on this PC; the free official demo is the only data source used for tests (legal terms recorded in the plan).
- The legacy OpenGL path (ARB/fixed function) runs through the Windows OpenGL driver on this GPU; if not, rendering failure is recorded and not silently ignored.
- Sound and networking may be stubbed or disabled for the first milestone.
- Missing `glprogs` shader sources (not in the GPL release) may block the ARB2 path; the plan decides the fallback renderer.
- Scope excludes the web port, gameplay fidelity, multiplayer, and AI modding; those are later specs.
- Source of truth for the analysis is `doom3_orig/doc/*.md`; line numbers drift after submodule bumps.
