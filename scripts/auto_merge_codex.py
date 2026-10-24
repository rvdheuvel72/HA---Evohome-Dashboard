#!/usr/bin/env python3
"""Safely merge an internal main PR ONLY after full CI and SECOND Codex review."""
import json
import os
import sys
from urllib.request import Request, urlopen

from check_codex_review import (check_final_review, request, pages,\n                                arduino_validation_required, ha_validation_required)


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


def dispatch_followups(token, repo, changed):
    """GITHUB_TOKEN merges do not fire push workflows; dispatch explicitly.

    A workflow_dispatch invoked with GITHUB_TOKEN *does* create workflow
    runs. These protected main-only release/validation workflows already
    support workflow_dispatch and independently enforce readiness checks.
    """
    workflows = []
    if arduino_validation_required(changed):
        workflows.append("ramses-arduino-wroom32e.yml")
    # Never re-publish an existing immutable beta after a CI-policy-only PR.
    # Firmware PRs that change the release/version contract dispatch a
    # main-branch publisher which itself validates tag uniqueness.
    if any(path.startswith("firmware/arduino-wroom32e/") for path in changed):
        workflows.append("publish-ramses-arduino-v0.4-beta.yml")
    if ha_validation_required(changed) or any(
            f.startswith(".github/workflows/") or f.startswith("scripts/")
            for f in changed):
        workflows.append("phase1-self-hosted.yml")
    for workflow in workflows:
        path = f"/repos/{repo}/actions/workflows/{workflow}/dispatches"
        # request() uses POST; GitHub returns 204 with no JSON body.
        _post_empty(token, path, {"ref": "main"})
        print("Dispatched post-merge workflow:", workflow)


def _post_empty(token, path, payload):
    url = "https://api.github.com" + path
    req = Request(url, data=json.dumps(payload).encode(), method="POST", headers={
        "Authorization": "Bearer " + token,
        "Accept": "application/vnd.github+json",
        "Content-Type": "application/json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "evohome-post-merge-dispatch",
    })
    with urlopen(req, timeout=25) as response:
        if response.status not in (201, 204):
            raise RuntimeError("Post-merge dispatch was not accepted")



def main():
    with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as f:
        event = json.load(f)
    pr = event.get("pull_request")
    repo = os.environ["GITHUB_REPOSITORY"]
    token = os.environ["GITHUB_TOKEN"]
    if pr is None and (event.get("issue") or {}).get("pull_request"):
        author = ((event.get("comment") or {}).get("user") or {}).get("login", "").lower()
        if author not in ("chatgpt-codex-connector", "chatgpt-codex-connector[bot]"):
            raise RuntimeError("Untrusted issue commenter")
        pr = request(token, f"/repos/{repo}/pulls/{event['issue']['number']}")
    if not pr:
        raise RuntimeError("Only protected PR or genuine Codex events permitted")
    sha = pr["head"]["sha"]
    validate(token, repo, pr["number"], sha)
    files = pages(token, f"/repos/{repo}/pulls/{pr['number']}/files")
    changed = [entry["filename"] for entry in files]
    merge(token, repo, pr["number"], sha)
    dispatch_followups(token, repo, changed)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("Automatic merge BLOCKED: " + str(error), file=sys.stderr)
        sys.exit(1)
