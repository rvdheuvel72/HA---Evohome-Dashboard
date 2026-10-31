"""Trust gate for stable HA releases.

A passing main-branch test run alone is NOT sufficient. Publication also
requires an actual merged PR, an exact-head Codex review without P1/P2,
and successful required PR checks (not a quota-deferred review).
"""
from __future__ import annotations

import os
import re

from check_codex_review import blockers, pages, request, required_ci_state, review_ready
from check_release_readiness import matching_merged_prs


def assert_release_ready(token: str, repository: str, merged_sha: str) -> int:
    if not token or not re.fullmatch(r"[a-fA-F0-9]{40}", merged_sha):
        raise ValueError("Missing GitHub token or exact merge SHA")
    prs = pages(token, f"/repos/{repository}/commits/{merged_sha}/pulls")
    merged = matching_merged_prs(prs, merged_sha)
    if len(merged) != 1:
        raise ValueError("HA release must correspond to exactly one reviewed PR merged into main")
    pr = merged[0]
    number = pr["number"]
    head = pr["head"]["sha"]
    details = request(token, f"/repos/{repository}/pulls/{number}")
    if (not details.get("merged_at")
            or details.get("merge_commit_sha") != merged_sha
            or details["head"]["sha"] != head
            or details["base"]["ref"] != "main"):
        raise ValueError("Merged PR provenance changed")
    owner, name = repository.split("/", 1)
    problems = blockers(token, owner, name, number, head)
    if problems:
        raise ValueError(f"PR #{number} has unresolved P1/P2 findings")
    state, explanation = required_ci_state(token, repository, head, pr_number=number)
    if state != "passed":
        raise ValueError(f"PR #{number} CI not fully green: {explanation}")
    reviews = pages(token, f"/repos/{repository}/pulls/{number}/reviews")
    comments = pages(token, f"/repos/{repository}/issues/{number}/comments")
    if not review_ready(reviews, comments, head):
        raise ValueError(f"PR #{number} has no completed clean exact-head Codex review")
    return number


def main() -> None:
    token = os.environ["GH_TOKEN"]
    repository = os.environ["GITHUB_REPOSITORY"]
    merged_sha = os.environ["VERIFIED_SHA"]
    number = assert_release_ready(token, repository, merged_sha)
    print(f"Stable HACS release provenance PASS: merged PR #{number}, exact-head "
          "Codex P1/P2 completed, required CI green, no unresolved findings")


if __name__ == "__main__":
    main()
