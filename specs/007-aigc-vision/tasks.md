# Tasks: AIGC Modern Vision

**Input**: `specs/007-aigc-vision/spec.md`, `plan.md`

- [x] T001 Author this spec/plan/tasks
- [x] T002 Generate page 2 with GPT Image 2.5 `images/edits` from the local official promo
- [x] T003 Generate page 3 with GPT Image 2.5 `images/generations`
- [x] T004 Crop to 16:9, resize 1600×900 JPEG q86, add `assets/aigc/README.md` provenance
- [x] T005 Extend `build.py`: default AIGC data, `--no-aigc`, reuse default redacted hero for page 2
- [x] T006 Page 2 DOM/CSS/runtime: `#p2New`, hide `#fx2` when AIGC, AIGC label, keep split/auto
- [x] T007 Page 3 DOM/CSS/runtime: `#p3Bg`, hide `#fx3` when AIGC, readable shade, AI meta label
- [x] T008 Page 2 old-side source: `DATA.archival` or default `DATA.hero`; licensed hero keeps redacted
- [x] T009 Update `verify.py` / `verify_deep.py` for images, labels, page 2 controls, page 3 content
- [x] T010 Run build, contract, deep, axe, reproducibility, 1920/4K screenshots
- [x] T011 Update `showcase/README.md` (AIGC, provenance, `--no-aigc`, licensing)
- [ ] T012 Commit, push, open PR with `Refs #2`, merge after review, comment on issue #2 (keep OPEN)
- [x] T013 Add the missing page-3 `AI-GENERATED VISION · NOT A GAME CAPTURE` truth label and
  assert the exact labels on both pages (self-review finding; FR-004)
- [x] T014 Harden the verifiers: exact data URI, decoded 1600×900, visual clip vs `aria-valuenow`,
  pointer drag, auto 72→40, page-3 cards above the image, 4K posters 2/3
- [x] T015 Run the axe matrix (0 violations; 120/66/113 incomplete) and the 58-check variant matrix
  (`--no-aigc`, `--no-hero`, `--hero`); record the evidence in `review.md`
- [x] T016 Commit the repeatable gates and finish the self-review fixes: `verify_variants.py`,
  `verify_axe.py`, build-time procedural labels for `--no-aigc`, static `hidden` on both AIGC
  images, clarified `--no-hero` help, page-3 truth-label contrast plate, regenerated `preview.png`

## Completion

| Bucket | Count |
|---|---|
| Done | 15 |
| Pending before review | 0 |
| Pending review/merge (T012) | 1 |
| Pending release/licensing (T042, issue #2) | 1 |

## Acceptance mapping

| Spec | Tasks |
|---|---|
| FR-001, FR-005 | T002–T005, T011 |
| FR-002, FR-004 | T006, T008, T009, T013 |
| FR-003, FR-004 | T007, T009, T013 |
| FR-006 | T005, T008, T010, T015 |
| FR-007 | T004, T010 |
| FR-008 | T009, T010, T014 |
| SC-001–SC-006 | T009, T010, T014, T015 |
