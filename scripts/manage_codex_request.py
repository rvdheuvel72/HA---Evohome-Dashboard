#!/usr/bin/env python3
"""Idempotently request pre-CI and post-CI Codex reviews for an exact PR SHA.

Runs ONLY on a protected-base pull_request_target workflow. No PR source
checkout, self-hosted machine, custom PAT or untrusted code execution.
"""
import json
import os
import sys

from check_codex_review import (check_ci, pages, request, review_disposition,
                                final_review_requested_at)


def ensure_request(token, repo, number, sha, phase):
    prefix = "/repos/" + repo
    pr = request(token, prefix + f"/pulls/{number}")
    if (pr["head"]["sha"] != sha or pr.get("state") != "open" or
            pr.get("draft") or
            pr["head"]["repo"]["full_name"] != repo or
            pr["base"]["ref"] != "main"):
        raise RuntimeError("Request blocked: not current, internal, open PR")
    comments = pages(token, prefix + f"/issues/{number}/comments")
    if phase == "pre":
        reviews = pages(token, prefix + f"/pulls/{number}/reviews")
        if review_disposition(reviews, comments, sha) == "reviewed":
            print("Codex pre-review already completed for exact head; no request")
            return
    if phase == "final":
        state, reason = check_ci(token, repo, number, sha)
        if state != "passed":
            raise RuntimeError("Cannot request final Codex before green CI: " + reason)
    if phase == "final":
        # A same-repo PR can forge a github-actions[bot] issue comment.
        # Only an authenticated successful protected final-request job can
        # suppress another genuine post-CI review request.
        if final_review_requested_at(token, repo, number, comments, sha):
            print("Verified protected post-CI review request already exists")
            return
    else:
        if any(f"<!-- codex-pre-review:{sha}:" in c.get("body", "")
               and (c.get("user") or {}).get("login", "").lower() in
                   ("github-actions[bot]", "github-actions")
               for c in comments):
            print("Codex pre-review already requested for exact head")
            return
    run_id = os.environ.get("GITHUB_RUN_ID")
    if not run_id or not run_id.isdecimal():
        raise RuntimeError("Missing protected workflow run identifier")
    marker = f"<!-- codex-{phase}-review:{sha}:run={run_id} -->"
    body = (
        f"@codex review\n\n{phase.upper()} review for exact HEAD "
        f"{sha}. Report all P1/P2; CI must be green before the final "
        f"review, and merging must await a new completed clean review.\n\n"
        + marker
    )
    created = request(token, prefix + f"/issues/{number}/comments", {"body": body})
    # Bind exact PR, SHA and actual created GitHub comment ID to the trusted
    # protected job logs. A forged github-actions[bot] comment cannot make
    # another PR's protected run attest its own comment ID.
    if phase == "final":
        comment_id = created.get("id")
        if not isinstance(comment_id, int):
            raise RuntimeError("Final review comment has no authenticated ID")
        print(f"CODEX_FINAL_REQUEST pr={number} sha={sha} comment_id={comment_id}",
              flush=True)
    print(f"Requested {phase} Codex review for {sha}")


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("pre", "final"):
        raise RuntimeError("Expected pre or final")
    with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as f:
        event = json.load(f)
    pr = event.get("pull_request")
    if not pr and (event.get("issue") or {}).get("pull_request"):
        actor = (event.get("comment") or {}).get("user") or {}
        if actor.get("login", "").lower() not in (
                "chatgpt-codex-connector", "chatgpt-codex-connector[bot]"):
            raise RuntimeError("Only authentic Codex completion comments can resume")
        repo = os.environ["GITHUB_REPOSITORY"]
        number = event["issue"]["number"]
        pr = request(os.environ["GITHUB_TOKEN"], f"/repos/{repo}/pulls/{number}")
    if not pr:
        raise RuntimeError("Only protected PR or trusted Codex completion events")
    ensure_request(os.environ["GITHUB_TOKEN"], os.environ["GITHUB_REPOSITORY"],
                   pr["number"], pr["head"]["sha"], sys.argv[1])


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("Codex review request blocked: " + str(error), file=sys.stderr)
        sys.exit(1)
