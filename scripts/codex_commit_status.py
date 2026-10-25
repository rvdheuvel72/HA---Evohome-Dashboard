#!/usr/bin/env python3
"""Exact-commit status for a notification-free, fail-closed Codex gate.

GitHub treats a skipped required Actions job as passing. A same-named
PENDING commit status prevents that job's skip from implying approval;
both contexts must pass when required by branch protection.
"""
import json
import os
import sys
from urllib.request import Request, urlopen

from check_codex_review import check_final_review, request

CONTEXT = "Codex P1/P2 review gate"


def post_status(token, repo, sha, state, number):
    if state not in ("pending", "success"):
        raise ValueError("Only pending or validated success status permitted")
    description = (
        "Awaiting exact-head Codex P1/P2 and relevant CI checks"
        if state == "pending" else
        "Both Codex reviews complete; required CI green; zero P1/P2"
    )
    body = json.dumps({
        "state": state,
        "context": CONTEXT,
        "description": description,
        "target_url": f"https://github.com/{repo}/pull/{number}",
    }).encode()
    path = f"https://api.github.com/repos/{repo}/statuses/{sha}"
    req = Request(path, method="POST", data=body, headers={
        "Authorization": "Bearer " + token,
        "Accept": "application/vnd.github+json",
        "Content-Type": "application/json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "evohome-codex-required-status",
    })
    with urlopen(req, timeout=20) as response:
        if response.status not in (200, 201):
            raise RuntimeError("GitHub did not accept required commit status")
    print(f"Exact commit {sha} status {CONTEXT}: {state}", flush=True)


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("pending", "success"):
        raise ValueError("Expected pending or success")
    with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as stream:
        event = json.load(stream)
    token = os.environ["GITHUB_TOKEN"]
    repo = os.environ["GITHUB_REPOSITORY"]
    event_pr = event.get("pull_request")
    if event_pr:
        number = event_pr["number"]
    else:
        issue = event.get("issue") or {}
        if not issue.get("pull_request"):
            raise RuntimeError("Not an authenticated pull-request event")
        number = issue["number"]
    pr = request(token, f"/repos/{repo}/pulls/{number}")
    if pr.get("state") != "open" or pr.get("draft"):
        print("Inactive PR; commit status unchanged")
        return
    if (pr["head"]["repo"]["full_name"] != repo or
            pr["base"]["ref"] != "main"):
        raise RuntimeError("Unsafe PR source or target")
    sha = pr["head"]["sha"]
    if event_pr and event_pr["head"]["sha"] != sha:
        print("Stale PR event; newer head owns the status")
        return
    if sys.argv[1] == "success":
        state, reason = check_final_review(token, repo, number, sha)
        if state != "passed":
            raise RuntimeError("Never mark green before final Codex: " + reason)
    post_status(token, repo, sha, sys.argv[1], number)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("Required Codex status cannot be published:",
              type(error).__name__, file=sys.stderr)
        sys.exit(1)
