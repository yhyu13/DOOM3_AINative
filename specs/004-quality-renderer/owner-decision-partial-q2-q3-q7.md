# Owner decision: Q2/Q3/Q7 accepted as partial / out of scope

**Date:** 2026-11-04
**Owner decision:** Accept Q2, Q3 and Q7 as **partial / out of scope for this programme**.
**Status:** Decision record only. No task row is marked complete by this file, and no issue or PR is
closed.

## What is accepted

| Stage | Accepted as | What is actually delivered |
|---|---|---|
| Q2 | **partial / out of scope** | Q2 adapter, ordered draw/texture/stencil, device consumer, texture cache, shadow-volume packets, fork call sites, residual tests and the independent T040 review record are merged. |
| Q3 | **partial / out of scope** | T085 engine sink/frame motion/linkage is merged; portable history/HUD tests and the Q3 residual work are merged; T044/T048 live pose/moving scene remain blocked. |
| Q7 | **partial / out of scope** | T072/T073 host profile/capability evidence and T075 review are merged; T074 real-scene run remains NOT RUN. |

This is an acceptance of the **scope**, not an acceptance of the missing acceptance criteria. Q2, Q3
and Q7 are **not** recorded as fully accepted.

## Rows that stay open

Do not tick these rows as a result of this decision:

| Stage | Open / blocked rows | Why |
|---|---|---|
| Q2 | T036, T038, T039, T080–T083 | live retail engine launch/parity, CP-07 data, failed-init real build |
| Q3 | T044, T048 | live pose / moving scene need a device consumer and retail md5/scene data |
| Q7 | T074 | no legal real-scene data; Q7 is not a web-support claim |

Q2 T040 is review-complete but its verdict is **Q2 BLOCKED**; Q3 T085 is done; Q7 T072/T073/T075 are
done. Those distinctions remain in `tasks.md`.

## Issues and PRs

- Issues #53 (Q2), #62 (Q3) and #67 (Q7) remain **open**.
- No PR is closed by this decision.
- Q5 T060–T062 remains **NOT RUN** (NVIDIA GPU/SDK/legal gates).
- 008/009 data-blocked rows remain blocked.

This decision can be revisited by the owner if legal retail data, a target NVIDIA system, or a
different acceptance scope becomes available.

## Evidence references

- `tasks.md` — current rows and blockers
- `review-q2.md` — T040 verdict, Q2 BLOCKED
- `review-q7.md` / `review-q7` record — Q7 limits and T074 NOT RUN
- `ledger-q2-q3-reconcile.md`
- Q2/Q3/Q6/Q7 evidence under `evidence/`
