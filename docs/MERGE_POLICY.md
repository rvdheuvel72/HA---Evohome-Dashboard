# Merge policy: required Codex P1/P2 validation

Do not merge into main before:
1. All self-hosted build/unit/runtime-contract checks have passed on the latest PR head.
2. Codex has reviewed the exact PR head, or an authentic recent Codex quota notice qualifies for the documented review-debt exception.
3. Every Codex P1 or P2 finding, including findings on earlier PR commits, is **fixed, regression-tested and its review thread resolved**.
4. After fixes, request review of the new head with `@codex review` and the **full 40-character head SHA**; record any quota-deferred review for the next run.
5. The isolated GitHub-hosted "Codex P1/P2 review gate" check is green (build/runtime tests remain on self-hosted runners).
6. The exact final head SHA is verified again before merge, then post-merge CI is checked.

CI gate implementation: the pull_request_target workflow is loaded from the protected default branch, checks out only the trusted base commit, and runs scripts/check_codex_review.py from that base (never from PR source) on an isolated GitHub-hosted runner. We intentionally do not run this token-bearing check on persistent self-hosted runners used by PR-controlled validation jobs. GitHub review records, the Codex completed summary identifying the exact head SHA, and unresolved P1/P2 review threads (GraphQL). Only a verified recent Codex **quota notice** permits a deferred review, never an unresolved P1/P2, failed CI or GitHub API error. Findings are never automatically suppressed or resolved.

**Bootstrap limitation:** Because the trusted check workflow is being introduced by the current PR, it cannot run from main for that same PR until merged. The bootstrap merge must be manually checked: fully completed Codex review of the final head with zero outstanding P1/P2, plus green existing CI. Future PRs use the protected-base gate.

**Enforcement requires GitHub repository rules**: Settings > Rules > Rulesets: target main, require PRs, disallow bypass, require status checks:
- "Python contract and dashboard syntax"
- "Pinned evohome-async runtime contract"
- "Codex P1/P2 review gate"

The currently connected GitHub app has no branch-protection update capability. The CI gate is implemented in code, but cannot force a human with merge/admin privileges to honor it until a ruleset is enabled.

## Baseline audit (2026-10-11)

PR #1 contained 3 P1 Codex findings (invalid setup API, invalid status poll method, incorrect schedule task wait), fixed via PR #2. PR #2 contained an unaddressed P2 finding after merge: expiry not verified during repeated temporary overrides. Follow-up branch fixes zone, hot-water and system-mode expiry checks, adds expiry tests, and ensures new PRs are blocked until Codex review completes.

No Home Assistant instance or live controller was accessed by these tests.

## Codex quota-limit exception — approved 2026-10-14

When Codex reports that its review usage limit is reached, allow a merge or
release after successful CI and zero unresolved P1/P2 issues.

The exception requires a real Codex bot quota message following a
review request that identifies the current full PR head SHA. A stale
quota message or an ordinary incomplete review does not qualify.

Record the deferred review as an open GitHub issue linking the PR,
specific head SHA and any affected beta release. On the next run
where Codex reviews are available, include the outstanding review
in that run and address any newly reported P1/P2 issues. Never
claim the deferred review was completed.

## Release policy

Release publishers verify source CI and all known P1/P2 findings.
They may accept an authenticated, recent quota deferral instead of
a completed latest-head review. An unresolved P1/P2, failed CI or
unverified quota condition still blocks publication.

Distribute firmware through GitHub Releases, not Actions artifacts.

## Final gate execution order (2026-10-16)

The Codex gate is a separate, protected `pull_request_target` workflow, not the first job inside the self-hosted test workflow. The workflow now has two dependent GitHub-hosted jobs: **Await required build and test checks** (source/HA checks, plus the Arduino compile for firmware/publisher changes), then **Codex P1/P2 review gate**. The second job uses `needs` and `always()`: failed or missing preflight is a hard failure rather than a skipped check that GitHub might treat as passing. The final job rechecks CI, exact-head Codex review or authenticated quota deferral, and every existing P1/P2 review thread. Codex findings are never suppressed; pre-existing unresolved findings still block release even if all CI is green.

Codex review comments and PR head updates may trigger a new gate run; only a fully successful latest-head final check permits merge. The privileged workflow continues to execute only code from the protected base branch on isolated GitHub-hosted runners, never unreviewed PR source code.


## Trusted firmware-build provenance (Codex P1 hardening, 2026-10-16)

For a firmware/publisher PR, final approval additionally requires a **Trusted Arduino compile** check from the protected-base `pull_request_target` workflow at `.github/workflows/trusted-ramses-compile.yml`. Unlike the PR-editable `pull_request` workflow, this definition comes from protected `main`, checks out the exact PR head, and compiles the real Arduino sketch on an isolated GitHub-hosted runner with only read-only repository permissions and no injected secrets. The trusted compiler is separate from the token-bearing review job.

The review checker validates the check's GitHub Actions identity, job/run URL and ID, trusted workflow path, `pull_request_target` event, repository and exact head SHA before accepting a successful compile. A same-named status reported from a PR-controlled workflow never satisfies this requirement. Post-merge release readiness applies the same check.

**Bootstrap:** this new protected compile workflow cannot run for the PR that first introduces it; GitHub loads `pull_request_target` workflows from the base branch. Bootstrap acceptance requires existing green CI, exact-head Codex review with all P1/P2 resolved, and static/regression validation. Subsequent firmware PRs must pass the protected compiler check.
