# Merge policy: required Codex P1/P2 validation

Do not merge into main before:
1. All self-hosted build/unit/runtime-contract checks have passed on the latest PR head.
2. Codex has completed its review of the latest PR head (not an earlier commit).
3. Every Codex P1 or P2 finding, including findings on earlier PR commits, is **fixed, regression-tested and its review thread resolved**.
4. After any fix commit, request a fresh Codex review on the new PR head. Use the PR comment "@codex review".
5. The isolated GitHub-hosted "Codex P1/P2 review gate" check is green (build/runtime tests remain on self-hosted runners).
6. The exact final head SHA is verified again before merge, then post-merge CI is checked.

CI gate implementation: the pull_request_target workflow is loaded from the protected default branch, checks out only the trusted base commit, and runs scripts/check_codex_review.py from that base (never from PR source) on an isolated GitHub-hosted runner. We intentionally do not run this token-bearing check on persistent self-hosted runners used by PR-controlled validation jobs. GitHub review records, the Codex completed summary identifying the exact head SHA, and unresolved P1/P2 review threads (GraphQL). If Codex is unavailable, the review has not completed, a thread is unresolved, or GitHub permissions fail, the gate stays red. It will not automatically suppress or resolve findings.

**Bootstrap limitation:** Because the trusted check workflow is being introduced by the current PR, it cannot run from main for that same PR until merged. The bootstrap merge must be manually checked: fully completed Codex review of the final head with zero outstanding P1/P2, plus green existing CI. Future PRs use the protected-base gate.

**Enforcement requires GitHub repository rules**: Settings > Rules > Rulesets: target main, require PRs, disallow bypass, require status checks:
- "Python contract and dashboard syntax"
- "Pinned evohome-async runtime contract"
- "Codex P1/P2 review gate"

The currently connected GitHub app has no branch-protection update capability. The CI gate is implemented in code, but cannot force a human with merge/admin privileges to honor it until a ruleset is enabled.

## Baseline audit (2026-10-11)

PR #1 contained 3 P1 Codex findings (invalid setup API, invalid status poll method, incorrect schedule task wait), fixed via PR #2. PR #2 contained an unaddressed P2 finding after merge: expiry not verified during repeated temporary overrides. Follow-up branch fixes zone, hot-water and system-mode expiry checks, adds expiry tests, and ensures new PRs are blocked until Codex review completes.

No Home Assistant instance or live controller was accessed by these tests.

## Codex review quota exception (repository-owner authorized, 2026-10-11)

Codex may post an authenticated bot comment saying its code-review quota is exhausted.
The trusted-base gate may then pass **without a Codex review** only when all of
these independently verified conditions hold:

- A genuine `chatgpt-codex-connector[bot]` quota-exceeded comment was created
  in the repository during the previous **six hours**. A PR author's text or
  similar username cannot enable this exception. The six-hour window expires
  automatically, returning to normal mandatory review.
- The exact PR head has **all three** CI checks green: Python/dashboard syntax,
  pinned `evohome-async` API contract, and Home Assistant runtime imports.
  Failed, pending or missing checks cannot be bypassed.
- There are zero blocking Codex P1/P2 review threads, including current-head
  findings even if someone resolved the GitHub thread.
- The PR is still open and the head SHA has not changed.

This exception never automatically merges a PR; other build/review policies
and manual approval remain applicable. No use of this exception can justify
merging a failing Renovate major-update PR.

The `issue_comment` trigger is needed because Codex edits its review summary.
Only verified Codex bot comments execute the gate; PR author/maintainer comments
cannot start extra review runs. A closed PR is ignored even for older queued
Codex events.
