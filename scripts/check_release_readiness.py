#!/usr/bin/env python3
"""Fail closed except verified review-quota deferral; never bypass P1/P2 or CI.

Publishers MUST run this on trusted main after merge, before creating a release.
Using the commit-to-associated-pull-requests API ties approval to exact source.
"""
from __future__ import annotations

import os
import sys

from check_codex_review import blockers, pages, request, required_ci_state, review_disposition


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
        disposition = review_disposition(reviews, comments, head)
        if disposition == "missing":
            raise RuntimeError(
                f"PR #{number} has neither completed Codex review nor verified "
                f"quota notice for exact final head {head}"
            )
        if disposition == "quota_deferred":
            # Publication is allowed under the user-approved quota exception,
            # but missing review must be visible in both logs and release notes.
            print(f"REVIEW DEBT: PR #{number} exact head {head} has an "
                  "uncompleted Codex review (verified quota notice). "
                  "Review on next available run.", flush=True)
        verified.append(number)
    return verified


def main():
    token = os.environ.get("GITHUB_TOKEN") or os.environ["GH_TOKEN"]
    repo = os.environ["GITHUB_REPOSITORY"]
    sha = os.environ["GITHUB_SHA"]
    for pr in verify(token, repo, sha):
        print(f"Release gate PASS: merged PR #{pr}; latest-head Codex "
              "review completed OR exact-head quota deferral recorded; "
              "zero unresolved P1/P2; required CI success", flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        # Avoid dumping raw GitHub API responses or authentication tokens.
        print("Release gate BLOCKED: " + str(error), file=sys.stderr)
        sys.exit(1)
