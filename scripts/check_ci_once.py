#!/usr/bin/env python3
"""One-shot exact-head CI inspection; no failure for pending external builds."""
import json
import os
import sys

from check_codex_review import check_ci, request


def main():
    with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as stream:
        event = json.load(stream)
    repo = os.environ["GITHUB_REPOSITORY"]
    token = os.environ["GITHUB_TOKEN"]
    pr = event.get("pull_request")
    if pr is None:
        issue = event.get("issue") or {}
        if not issue.get("pull_request"):
            raise RuntimeError("Expected an authenticated PR event")
        pr = request(token, f"/repos/{repo}/pulls/{issue['number']}")
    state, reason = check_ci(token, repo, pr["number"], pr["head"]["sha"])
    print("CI:", state, reason)
    if state == "passed":
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as out:
            out.write("approved=true\n")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("Cannot inspect CI:", type(error).__name__, file=sys.stderr)
        sys.exit(1)
