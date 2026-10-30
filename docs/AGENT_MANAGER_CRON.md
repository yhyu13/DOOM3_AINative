# Agent Manager cron orchestration

How a coordinator agent runs the dependency graph across Agent Manager sessions with a recurring
cron. This document is a workflow summary. `AGENTS.md` and `docs/GIT_WORKFLOW.md` are authoritative;
if they disagree with this page, they win.

## 1. Purpose and scope

The cron is a lightweight scheduler for issue-scoped Agent Manager work:

- inspect active sessions and open PRs;
- merge only what already has an independent review and every merge gate;
- send exact blockers back to the author;
- start the next unblocked node, or nothing.

It is **not** an automatic DAG engine. The coordinator reads the graph, chooses the wave, and re-plans
when a dependency returns.

## 2. Roles

| Role | Responsibilities | Must not |
|---|---|---|
| Coordinator (cron session) | read `agent_manager`/PRs, verify evidence, merge or hold, start at most one new disjoint session | implement a worker ticket, self-review, merge stale approvals |
| Author session | one issue, one branch, one worktree, one PR; fix blockers; request re-review | review own work, close issues, duplicate another writer |
| Independent reviewer | own Agent Manager session/worktree; read diff/specs/command; rerun; record verdict with session ID and reviewed SHA | be the author, share the author's worktree, be prompted for a predetermined verdict |
| Human owner | final authority for submodule bumps, policy changes, stop instructions | — |

Maximum 3–4 active sessions. One writer per file. Agents may merge their own PR only under
`AGENTS.md` §11.

## 3. Per-tick loop

1. `agent_manager list` — get session/worktree IDs and activity.
2. `gh pr list --state open` — identify candidates.
3. For each candidate, read `headRefOid`, `mergeable`, `mergeStateStatus`, comments and reviews.
4. Classify:
   - **No independent APPROVED at current head** → request review; do not merge.
   - **BLOCKED** → send exact `file:line` blockers to the author; do not merge.
   - **APPROVED** → run merge gates (§4).
5. Merge only if every gate passes; verify the merge commit and main.
6. Update the graph; start at most one new session whose inputs are already on main and whose files
   are disjoint.
7. Report material changes only (SHAs, verdicts, blockers, merges). Do not narrate routine waiting.

## 4. Merge gates

All must hold immediately before merge:

- independent non-author review recorded in a PR comment with reviewer session ID and reviewed SHA;
- PR head still equals the reviewed SHA;
- merge base of reviewed SHA and current `origin/main` equals current `origin/main`;
- `MERGEABLE` / `CLEAN`; required checks/verification pass;
- local main worktree clean and current; `git merge-tree` shows no conflict;
- no out-of-scope paths, game data, build output or unexplained submodule dirt;
- no issue is closed without permission;
- owner sign-off present when required (submodule bumps; build-system until the self-merge policy is
  merged — check current `AGENTS.md`).

Any new commit, rebase, force-push or advance of `origin/main` **voids every other approval at an
older base**. Rebase the remaining PR and request a new review; do not merge on “still CLEAN”.

Commands:

```powershell
gh auth status
# If the active account is read-only (yhxd123), fix it:
gh auth switch --user yhyu13
gh auth setup-git

git fetch origin
git rev-parse origin/main
git merge-base origin/main <reviewed-sha>
gh pr view <N> --json headRefOid,mergeable,mergeStateStatus,statusCheckRollup
gh pr merge <N> --squash
```

Do not use `--delete-branch`: Agent Manager worktrees still reference the branch.

## 5. Review and fix loop

1. Reviewer posts `APPROVED` or `BLOCKED` at a SHA.
2. If BLOCKED, the coordinator sends the exact findings to the author; the author fixes only its
   owned files, pushes, and requests re-review.
3. A new SHA requires a new verdict. Do not reuse an earlier approval.
4. If the fix is documentation-only, the reviewer may re-review quickly, but still at the new SHA.

An approval comment must state the reviewed SHA and the reviewer session ID. A template, a
conditional sentence, or an author-written approval is not a verdict.

## 6. Dependency and file rules

- Follow the graph: `S2 → S8 → S9 → S10`; `S6`/`S7` feed `S10`; `S11`/`S12`/`S13` follow Q4.
- A node starts only when its inputs are on `main`, not merely open in a PR.
- Two sessions may run in parallel only with disjoint file ownership.
- Shared files (`src/renderer/rhi/**`, target CMake, `tests/renderer/CMakeLists.txt`) are serialised.
- When a dependency return changes an interface, stop the dependent writer, re-plan, and rebase.

## 7. Handling main advance (approval cascade)

Merging any PR advances `origin/main` and invalidates approvals on every other open PR. This is
expected, not a bug:

1. merge the one whose gates pass;
2. rebase the rest onto the new main;
3. request fresh reviews;
4. never merge the old approval.

If the remaining PR was already reviewed and only the base moved, the reviewer still must verify the
new SHA and base; a “clean” GitHub status is not enough.

## 8. Stalled or conflicting sessions

A session is **stalled** when its files have not changed for ~30–45 minutes and it has no commit or
PR. Do not start a duplicate writer.

1. Preserve its work as a WIP commit on its branch and push.
2. Start **one** new Agent Manager session that cherry-picks/merges the WIP into a new branch.
3. Keep the old worktree as an archive; never edit it again.
4. Record the WIP SHA in the new session's prompt.

If a session is merely busy, leave it alone. If two writers touch the same file, stop one and
serialise.

## 9. Owner gates

- **Submodule bumps** (`doom3_orig/doom3`): human owner sign-off is required. Record it as a PR
  comment from the owner or an `owner-sign-off:` line quoting that comment.
- **Build-system changes**: check current `AGENTS.md` §11. The policy is moving toward
  self-merge after an independent reviewer reruns the build/tests and records the evidence; until
  that policy is merged, the owner sign-off requirement still applies.
- **Policy changes and stopping work**: owner only. A cron message is not owner authorization.

Owner sign-off must be unconditional and name the reviewed SHA. A request template is not a sign-off.

## 10. Cron configuration

- Cadence: 5 minutes (`*/5 * * * *`), bounded to roughly 5 minutes of work per tick.
- If the only blocker is a human decision, **pause the cron** (`cron_delete`) instead of repeating
  no-op checks; resume after the owner responds.
- Keep one cron task. `cron_list` shows the current id; recreate with `cron_create` after changing the
  prompt.
- The prompt must say: merge only approved/current/gated PRs; send blockers; do not duplicate writers;
  start at most one new session; verify `gh` auth; report material changes only.

## 11. Reporting format

Report only material changes:

```text
PR #N  [APPROVED/BLOCKED/MERGED]  reviewed SHA  base  blocker/merge commit
DAG: node [active/done/blocked] -> next
```

No “done” without a command/PR evidence. No routine “still waiting” unless the state changed.
Include exact SHAs, paths and exits when they are the evidence.

## 12. New manager checklist

1. Read `AGENTS.md`, `docs/GIT_WORKFLOW.md`, `CONTRIBUTING.md`.
2. `cron_list` and `agent_manager list`; identify the coordinator cron and active sessions.
3. `git fetch origin`; confirm main is current; check `gh auth status`.
4. Read open PR comments/reviews; classify each as no-review / BLOCKED / APPROVED.
5. Run merge gates; merge only the gated ones; verify the merge commit.
6. Update the graph; start at most one unblocked, disjoint session.
7. If blocked only on a human, pause the cron and state the exact question.
8. Before ending a shift, leave the graph, open PRs, blockers and next action in the final report.

## 13. Snapshot (2026-10-30) — verify, do not assume

- S2 resolver wiring merged at `2487baf`; S8 Q2 adapter merged at `9bb40c9`.
- S9 Q3 integration is active on `feat/q3-motion-hud-integration`.
- Policy PR #61 (build-system self-merge after review) is under review.
- S10 waits for S9 and S7; S11/S12/S13 wait for Q4.
- This snapshot is historical; always re-read the repository.

## 14. References

- `AGENTS.md` — non-negotiables, §11 merge authority.
- `docs/GIT_WORKFLOW.md` — roles, §8 merge policy, finish checklist.
- `CONTRIBUTING.md` — step 10, definition of done.
- `specs/004-quality-renderer/tasks.md` — Q0–Q7 gates.
- `specs/004-quality-renderer/q2-adapter-brief.md`, `classic-parity.md`,
  `engine-integration.md`, motion fixtures — current dependency details.
- `docs/SKILLS.md` — available review, worktree, verification and debugging skills.
