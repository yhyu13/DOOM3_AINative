# Specification Quality Checklist: Rendering modernization path — [historical draft](../spec.md)

**Purpose**: Validate specification completeness and quality before planning
**Created**: 2026-10-27
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No unresolved implementation choices are hidden as assumptions — the seven proposed decisions are named and
      marked "Proposed", with reversal conditions
- [x] Focused on user value: a base that builds, provable parity, a legal material win, and reach without a
      rewrite
- [x] Readable by a non-specialist stakeholder: problem, decisions, scenarios, and success criteria are stated
      before the technical evidence
- [x] All mandatory sections completed
- [ ] No implementation details — **exception noted**: this is an architecture-decision spec, so GL 3.3, ES 3.00,
      Vulkan, DX12, BC7, and DLSS are scope boundaries, not implementation leakage. The *how* stays in
      `research.md` and the future `plan.md`.

## Requirement Completeness

- [x] No `[NEEDS CLARIFICATION]` markers remain
- [x] Requirements are testable and unambiguous: each FR names an artifact, a check, or a build property
- [x] Success criteria are measurable: build exit code, PE machine type, mean absolute error, FPS, VRAM budget
- [x] Success criteria are technology-agnostic where the outcome allows it; technology names appear only where
      they are the decision under test (backend choice, upscaler legality)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified, including missing data, BFG vs 2004 data, custom ARB mods, missing GPU
      features, mirrors/subviews, and ABI breakage
- [x] Scope is clearly bounded: out-of-scope section excludes gameplay, D3D12, console, iOS shipping, and
      proprietary SDKs
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria or a named scenario
- [x] User scenarios cover the primary flows: base decision, baseline/measurement, material-only, GL parity,
      second backend, web, AI gate, and optional neural evaluation
- [x] Feature meets measurable outcomes defined in Success Criteria
- [ ] No implementation details leak into the specification — same documented exception as above

## Validation Notes

- The spec deliberately makes **D1 (engine base)**, **D4/D6 (Vulkan/DX12 + DLSS 5)**, and **D7 (D3HDP as local
  data)** the decisions that require explicit approval. They change provenance, architecture, and legal posture,
  not just code.
- Performance numbers (10% parity, 30 FPS web, 16–32 engineer-weeks) are placeholders or engineering-judgement
  ranges because no GPU is named and no baseline exists. They are recorded as gates to be ratified in `plan.md`
  after SC-004 measures the hardware.
- The DLSS 5 gate is deliberately **experiment-first**: a 1–2 day external A/B on the 32-bit build must precede
  any native-backend investment, and a missing RTX 50 GPU is SKIPPED, not PASS.
- The material track is independent of the base decision: it can start on the current engine and move to the new
  base without rework, provided the manifest and RXGB/DXT rules are followed.
- Legal statements are engineering constraints and are not legal advice.

## Result

**Historical result**: Superseded by [004](../../004-quality-renderer/spec.md); this draft is not ready for `/speckit.clarify` or `/speckit.plan`.
The two unchecked boxes are the documented architecture-decision exception, and the schedule estimates in the body are withdrawn by the accuracy notice in [spec.md](../spec.md).
