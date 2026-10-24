#!/usr/bin/env python3
"""Restart failed PR review gates after a genuine late Codex completion.

Protected-base, GitHub-hosted only. No unreviewed code executes.
"""
import json
import os
import sys
from urllib.request import Request, urlopen
from check_codex_review import check_review_only, request

WORKFLOWS = (
    ("ramses-arduino-wroom32e.yml", "pull_request"),
    ("phase1-self-hosted.yml", "pull_request"),
    ("trusted-ramses-compile.yml", "pull_request_target"),
)


def post_retry(token, path):
    req = Request("https://api.github.com" + path, data=b"{}",
                  method="POST", headers={
        "Authorization": "Bearer " + token,
        "Accept": "application/vnd.github+json",
        "Content-Type": "application/json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "evohome-late-codex-resume",
    })
    with urlopen(req, timeout=25) as response:
        if response.status not in (201, 202, 204):
            raise RuntimeError("GitHub rejected workflow retry")


def resume(token, repo, number, sha):
    state, reason = check_review_only(token, repo, number, sha)
    if state != "passed":
        print("No CI restart: " + reason)
        return
    for filename, event in WORKFLOWS:
        base = f"/repos/{repo}/actions/workflows/{filename}/runs"
        response = request(token, base + f"?event={event}&per_page=100")
        runs = response.get("workflow_runs", [])
        matching = [
            x for x in runs
            if x.get("status") == "completed"
            and x.get("conclusion") == "failure"
            and any(p.get("number") == number and
                    ((p.get("head") or {}).get("sha") == sha or
                     x.get("head_sha") == sha)
                    for p in x.get("pull_requests", []))
        ]
        if not matching:
            print("No exact-head failed pre-review workflow to restart:", filename)
            continue
        latest = max(matching, key=lambda x: x.get("id", 0))
        post_retry(token, f"/repos/{repo}/actions/runs/{latest['id']}/rerun-failed-jobs")
        print("Restarted failed review-gated workflow", filename, latest["id"])


def main():
    with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as stream:
        event = json.load(stream)
    author = ((event.get("comment") or {}).get("user") or {}).get("login", "").lower()
    if author not in ("chatgpt-codex-connector", "chatgpt-codex-connector[bot]"):
        raise RuntimeError("Only authenticated Codex comments may resume CI")
    issue = event.get("issue") or {}
    if not issue.get("pull_request"):
        raise RuntimeError("Not a pull-request comment")
    repo = os.environ["GITHUB_REPOSITORY"]
    token = os.environ["GITHUB_TOKEN"]
    number = issue["number"]
    pr = request(token, f"/repos/{repo}/pulls/{number}")
    if (pr["head"]["repo"]["full_name"] != repo or
            pr["base"]["ref"] != "main" or pr.get("state") != "open"):
        print("Ignore stale, external or closed pull request")
        return
    resume(token, repo, number, pr["head"]["sha"])


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print("Review-triggered CI recovery failed:", type(exc).__name__)
        sys.exit(1)
