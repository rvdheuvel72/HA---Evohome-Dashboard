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

## Mandatory latest-head review — no quota exemption (2026-10-14)

A Codex quota notice does **not** waive the requirement for a completed,
exact-final-head Codex review. The P1/P2 gate remains red until that review
has finished, every identified P1/P2 is fixed, regression-tested, and
resolved, and the required CI checks have passed on the same head commit.

Never merge just because source tests passed while review was still pending.
Whenever a fix updates the PR head, request a new exact-head Codex review.

## Mandatory release gate

Every beta/stable GitHub Release must be published from the protected
default branch **after** merging reviewed source through a PR. The
publisher runs `scripts/check_release_readiness.py` before `gh release
create`; it looks up the merged PR associated with the exact published
commit and checks the final PR head for: completed Codex review, clean
P1/P2 findings, and required CI success. Missing data or errors fail closed.

Build validation and GitHub Releases are separate stages. **Do not
publish Actions artifacts as user-facing firmware downloads.**

A release can still be triggered by an administrator if repository
rulesets are absent. Configure Rulesets on main to require the exact-head
Codex review gate, CI checks, and no bypass as described above. There is
no GitHub repo-settings control exposed through the currently connected
app, so code-only gates cannot guarantee branch protection.
