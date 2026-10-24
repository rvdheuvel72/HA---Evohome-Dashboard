#!/usr/bin/env python3
"""Safely merge an internal main PR ONLY after full CI and SECOND Codex review."""
import json
import os
import sys
from urllib.request import Request, urlopen

from check_codex_review import check_final_review, request


def validate(token, repo, number, expected_sha):
    prefix = "/repos/" + repo
    pr = request(token, prefix + f"/pulls/{number}")
    if pr.get("state") != "open" or pr.get("draft"):
        raise RuntimeError("PR is closed or draft")
    if (pr["head"]["sha"] != expected_sha or
            pr["head"]["repo"]["full_name"] != repo or
            pr["base"]["ref"] != "main"):
        raise RuntimeError("Exact PR head or trusted base changed")
    state, reason = check_final_review(token, repo, number, expected_sha)
    if state != "passed":
        raise RuntimeError("Final required checks not green: " + reason)
    return True


def merge(token, repo, number, sha):
    url = f"https://api.github.com/repos/{repo}/pulls/{number}/merge"
    payload = json.dumps({"sha": sha, "merge_method": "merge"}).encode()
    req = Request(url, data=payload, method="PUT", headers={
        "Authorization": "Bearer " + token,
        "Accept": "application/vnd.github+json",
        "Content-Type": "application/json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "evohome-safe-merge",
    })
    with urlopen(req, timeout=25) as response:
        result = json.load(response)
    if not result.get("merged"):
        raise RuntimeError("GitHub did not confirm a completed merge")
    print("Merged reviewed exact head", sha, "as", result.get("sha"))


def main():
    with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as f:
        event = json.load(f)
    pr = event.get("pull_request")
    if not pr:
        raise RuntimeError("Only internal protected PR events permitted")
    sha = pr["head"]["sha"]
    repo = os.environ["GITHUB_REPOSITORY"]
    token = os.environ["GITHUB_TOKEN"]
    validate(token, repo, pr["number"], sha)
    merge(token, repo, pr["number"], sha)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("Automatic merge BLOCKED: " + str(error), file=sys.stderr)
        sys.exit(1)
