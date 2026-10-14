#!/usr/bin/env python3
"""Fail closed: no GitHub Release without final-head Codex review and clean P1/P2.

Publishers MUST run this on trusted main after merge, before creating a release.
Using the commit-to-associated-pull-requests API ties approval to exact source.
"""
from __future__ import annotations

import os
import sys

from check_codex_review import blockers, pages, request, required_ci_state, review_ready


def matching_merged_prs(prs, sha: str):
    return [
        p for p in prs
        if p.get("merged_at")
        and p.get("merge_commit_sha") == sha
        and (p.get("base") or {}).get("ref") == "main"
        and (p.get("head") or {}).get("sha")
    ]


def verify(token: str, full_repo: str, sha: str) -> list[int]:
    if not token or not sha or len(sha) != 40:
        raise RuntimeError("Token or exact 40-char release commit SHA missing")
    associated = pages(token, f"/repos/{full_repo}/commits/{sha}/pulls")
    matched = matching_merged_prs(associated, sha)
    if not matched:
        raise RuntimeError("Release commit is not an approved PR merge into main")
    owner, name = full_repo.split("/", 1)
    verified = []
    for pr in matched:
        number = pr["number"]
        head = pr["head"]["sha"]
        details = request(token, f"/repos/{full_repo}/pulls/{number}")
        # REST association can be stale: re-read and verify exact final PR head.
        if not details.get("merged_at") or details.get("merge_commit_sha") != sha:
            raise RuntimeError(f"PR #{number} merge information changed")
        if details["head"]["sha"] != head:
            raise RuntimeError(f"PR #{number} final head changed")
        pending = blockers(token, owner, name, number, head)
        if pending:
            raise RuntimeError(
                f"PR #{number} has unaddressed P1/P2: " + "; ".join(pending[:8])
            )
        state, reason = required_ci_state(token, full_repo, head)
        if state != "passed":
            raise RuntimeError(f"PR #{number} required CI not passed: {reason}")
        prefix = f"/repos/{full_repo}"
        reviews = pages(token, prefix + f"/pulls/{number}/reviews")
        comments = pages(token, prefix + f"/issues/{number}/comments")
        if not review_ready(reviews, comments, head):
            raise RuntimeError(
                f"PR #{number} lacks completed Codex review of exact final head {head}"
            )
        verified.append(number)
    return verified


def main():
    token = os.environ["GITHUB_TOKEN"]
    repo = os.environ["GITHUB_REPOSITORY"]
    sha = os.environ["GITHUB_SHA"]
    for pr in verify(token, repo, sha):
        print(f"Release gate PASS: merged PR #{pr}, exact-head Codex, "
              "no unaddressed P1/P2, required CI success", flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        # Avoid dumping raw GitHub API responses or authentication tokens.
        print("Release gate BLOCKED: " + str(error), file=sys.stderr)
        sys.exit(1)
