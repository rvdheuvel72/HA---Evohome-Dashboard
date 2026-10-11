# Merge policy: required Codex P1/P2 validation

Do not merge into main before:
1. All self-hosted build/unit/runtime-contract checks have passed on the latest PR head.
2. Codex has completed its review of the latest PR head (not an earlier commit).
3. Every Codex P1 or P2 finding, including findings on earlier PR commits, is **fixed, regression-tested and its review thread resolved**.
4. After any fix commit, request a fresh Codex review on the new PR head. Use the PR comment "@codex review".
5. The self-hosted "Codex P1/P2 review gate" CI check is green.
6. The exact final head SHA is verified again before merge, then post-merge CI is checked.

CI gate implementation: scripts/check_codex_review.py checks GitHub review records, the Codex completed summary identifying the exact head SHA, and unresolved P1/P2 review threads (GraphQL). If Codex is unavailable, the review has not completed, a thread is unresolved, or GitHub permissions fail, the gate stays red. It will not automatically suppress or resolve findings.

**Enforcement requires GitHub repository rules**: Settings > Rules > Rulesets: target main, require PRs, disallow bypass, require status checks:
- "Python contract and dashboard syntax"
- "Pinned evohome-async runtime contract"
- "Codex P1/P2 review gate"

The currently connected GitHub app has no branch-protection update capability. The CI gate is implemented in code, but cannot force a human with merge/admin privileges to honor it until a ruleset is enabled.

## Baseline audit (2026-10-11)

PR #1 contained 3 P1 Codex findings (invalid setup API, invalid status poll method, incorrect schedule task wait), fixed via PR #2. PR #2 contained an unaddressed P2 finding after merge: expiry not verified during repeated temporary overrides. Follow-up branch fixes zone, hot-water and system-mode expiry checks, adds expiry tests, and ensures new PRs are blocked until Codex review completes.

No Home Assistant instance or live controller was accessed by these tests.
