# Capture index: what exists, and what is blocked

**Date**: 2026-10-29
**Spec**: [`../spec.md`](../spec.md) FR-011, FR-013, FR-015 (task T-031)

Every Q4 content claim needs a capture that names its backend and adapter, or an explicit `BLOCKED`
reason. This file is the index of those claims. It is deliberately short.

## Captures

| Cell | Material | Capture | Backend | Adapter | Reason |
|---|---|---|---|---|---|
| opaque dielectric | `hero_dielectric` | **BLOCKED** | — | — | spec 004 has not reached Q2/Q3/Q4; no RHI path renders a PBR material |
| opaque metal | — | **BLOCKED** | — | — | same, and no material is authored yet |
| alpha-tested geometry | — | **BLOCKED** | — | — | same |
| emissive | — | **BLOCKED** | — | — | same |
| skinned character | — | **BLOCKED** | — | — | same |
| transmissive / glass | — | **BLOCKED** | — | — | same |
| large environment surface | — | **BLOCKED** | — | — | same |

**Captures on file: 0.** Nothing in this workstream may claim otherwise, and `capture-q4.ps1` is the
only script allowed to produce one; it exits 3 today with `BLOCKED: spec 004 has not reached Q4`.

## Separation from the legacy track (FR-015)

Spec 006's HD result is a **legacy OpenGL** result and is not Q4 evidence. Two greps prove the content
workstream did not inherit it:

```text
$ git grep -n "HdTextures" -- content
(no output: no script or asset under content/ depends on the legacy switch)

$ git grep -n "image_usePrecompressedTextures" -- content
(no output: no pipeline step forces the legacy texture path)

$ git grep -n "HdTextures" -- specs/009-rhi-hd-content/evidence
specs/009-rhi-hd-content/evidence/captures.md: <this file, describing the rule>
```

The prose documents the rule; the commands prove the pipeline does not depend on it. Earlier versions of
this file quoted a grep over `specs/` as well, which matched the prose itself and proved nothing.

## What would change this file

1. Spec 004 delivers Q4 (which itself needs Q2/Q3).
2. `capture-q4.ps1` gains its pass path and writes `<material>/capture.png` plus a line naming the
   backend and adapter.
3. The matrix report (`content/tools/matrix-report.ps1`) then shows a capture instead of `BLOCKED` for
   that cell.
