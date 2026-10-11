"""Fail-closed P1/P2 Codex review gate for latest GitHub PR head."""
import json
import os
import re
import sys
import time
from urllib.request import Request, urlopen

TRUSTED_CODEX_LOGINS = {"chatgpt-codex-connector", "chatgpt-codex-connector[bot]"}
BLOCKER = re.compile(r"(?:!\[P[12] Badge\]|\[(?:P1|P2)\]|\bP[12] Badge\b)", re.I)
COMPLETED = "\u2705 **Completed**"


def is_codex(author):
    name = (author or {}).get("login", "").lower()
    return name in TRUSTED_CODEX_LOGINS


def is_p12(body):
    return bool(BLOCKER.search(body[:450]))


def request(token, path, data=None):
    headers = {"Authorization": "Bearer " + token,
               "Accept": "application/vnd.github+json",
               "X-GitHub-Api-Version": "2022-11-28",
               "User-Agent": "evohome-codex-gate"}
    if data is not None:
        headers["Content-Type"] = "application/json"
    url = "https://api.github.com" + path
    req = Request(url, data=json.dumps(data).encode() if data is not None else None,
                  headers=headers, method="POST" if data is not None else "GET")
    with urlopen(req, timeout=20) as resp:
        return json.load(resp)


def pages(token, path):
    out = []
    for page in range(1, 11):
        data = request(token, path + ("&" if "?" in path else "?")
                       + "per_page=100&page=" + str(page))
        if not isinstance(data, list):
            raise RuntimeError("Invalid GitHub response")
        out.extend(data)
        if len(data) < 100:
            return out
    raise RuntimeError("Review pagination limit hit")


def review_ready(reviews, summaries, sha):
    reviewed = any(is_codex(x.get("user")) and x.get("commit_id") == sha
                   and x.get("submitted_at") and x.get("state") != "PENDING"
                   for x in reviews)
    completed = any(is_codex(x.get("user"))
                    and "codex-pull-request-review-summary" in x.get("body", "")
                    and ("\u2705 **Completed**" in x.get("body", ""))
                    and (chr(96) + sha[:7] + chr(96)) in x.get("body", "")
                    for x in summaries)
    return reviewed and completed


def finding_blocks(thread, comment, sha):
    """A current-head P1/P2 blocks regardless of thread resolution."""
    if not is_codex(comment.get("author")) or not is_p12(comment.get("body", "")):
        return False
    original = (comment.get("originalCommit") or {}).get("oid")
    return original is None or original == sha or not thread["isResolved"]


def blockers(token, owner, repo, number, sha):
    query = """query($owner:String!,$repo:String!,$num:Int!,$cursor:String){
      repository(owner:$owner,name:$repo){pullRequest(number:$num){
        reviewThreads(first:100,after:$cursor){
          pageInfo{hasNextPage endCursor}
          nodes{isResolved comments(first:100){pageInfo{hasNextPage}
            nodes{body author{login} originalCommit{oid}}}}
        }}}}"""
    found = []
    cursor = None
    for _ in range(25):
        result = request(token, "/graphql", {"query": query,
            "variables": {"owner": owner, "repo": repo,
                          "num": number, "cursor": cursor}})
        if result.get("errors"):
            raise RuntimeError("Cannot check Codex review-thread resolution")
        threads = result["data"]["repository"]["pullRequest"]["reviewThreads"]
        for thread in threads["nodes"]:
            if thread["comments"]["pageInfo"]["hasNextPage"]:
                raise RuntimeError("Oversized review thread")
            for comment in thread["comments"]["nodes"]:
                if finding_blocks(thread, comment, sha):
                    found.append(comment["body"].splitlines()[0][:150])
        if not threads["pageInfo"]["hasNextPage"]:
            return found
        cursor = threads["pageInfo"]["endCursor"]
    raise RuntimeError("Review thread pagination limit hit")


def check(token, full_repo, number, sha):
    info = request(token, "/repos/" + full_repo + "/pulls/" + str(number))
    if info["head"]["sha"] != sha:
        return "changed", "PR head changed, check newest commit"
    prefix = "/repos/" + full_repo
    revs = pages(token, prefix + "/pulls/" + str(number) + "/reviews")
    summaries = pages(token, prefix + "/issues/" + str(number) + "/comments")
    if not review_ready(revs, summaries, sha):
        return "pending", "Waiting for completed Codex review of latest PR head"
    owner, name = full_repo.split("/", 1)
    unresolved = blockers(token, owner, name, number, sha)
    if unresolved:
        return "blocked", "Unresolved Codex P1/P2: " + "; ".join(unresolved[:8])
    return "passed", "Latest-head Codex review completed; no unresolved P1/P2"


def main():
    with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as stream:
        event = json.load(stream)
    token, full_repo = os.environ["GITHUB_TOKEN"], os.environ["GITHUB_REPOSITORY"]
    pr = event.get("pull_request")
    if pr is None and (event.get("issue") or {}).get("pull_request"):
        number = event["issue"]["number"]
        pr = request(token, "/repos/" + full_repo + "/pulls/" + str(number))
    if pr is None:
        raise RuntimeError("A PR event is required")
    expires = time.monotonic() + 480
    while True:
        result, message = check(token, full_repo, pr["number"], pr["head"]["sha"])
        print(message, flush=True)
        if result == "passed":
            return 0
        if result != "pending" or time.monotonic() + 15 >= expires:
            return 1
        time.sleep(15)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        # Do not expose GitHub authentication, raw API response or tokens.
        print("Codex gate failed to inspect review: " + type(exc).__name__, file=sys.stderr)
        sys.exit(1)
