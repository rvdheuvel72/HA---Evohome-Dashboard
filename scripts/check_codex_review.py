"""Fail-closed P1/P2 Codex review gate for latest GitHub PR head."""
import json
import os
import re
import sys
import time
from datetime import datetime, timedelta, timezone
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



# A recent, genuine Codex quota notification authorizes the repository owner's
# documented exception. The exception expires automatically after six hours.
QUOTA_NOTICE = "You have reached your Codex usage limits for code reviews."
QUOTA_MAX_AGE = timedelta(hours=6)
REQUIRED_CHECKS = (
    "Python contract and dashboard syntax",
    "Independent HACS frontend package",
    "Pinned evohome-async runtime contract",
    "Home Assistant 2026.10 runtime imports",
)


def recent_quota_notice(token, full_repo, *, now=None):
    """Use ONLY an authenticated GitHub Codex bot comment, never PR text."""
    now = now or datetime.now(timezone.utc)
    notices = request(
        token, "/repos/" + full_repo
        + "/issues/comments?sort=created&direction=desc&per_page=100"
    )
    if not isinstance(notices, list):
        raise RuntimeError("Cannot verify Codex quota status")
    for item in notices:
        if not is_codex(item.get("user")) or not item.get("body", "").startswith(QUOTA_NOTICE):
            continue
        try:
            stamp = datetime.fromisoformat(item["created_at"].replace("Z", "+00:00"))
        except (KeyError, TypeError, ValueError):
            continue
        if timedelta(0) <= now - stamp <= QUOTA_MAX_AGE:
            return True
    return False


def required_ci_state(token, full_repo, sha):
    """Never allow a quota exception to hide failed, missing, or pending CI."""
    response = request(
        token, "/repos/" + full_repo + "/commits/" + sha + "/check-runs?per_page=100"
    )
    checks = response.get("check_runs") if isinstance(response, dict) else None
    if not isinstance(checks, list):
        raise RuntimeError("Cannot inspect required CI check runs")
    for name in REQUIRED_CHECKS:
        matching = [run for run in checks if run.get("name") == name]
        if not matching:
            return "pending", "Waiting for required CI: " + name
        newest = max(matching, key=lambda run: (run.get("started_at") or "", run.get("id") or 0))
        if newest.get("status") != "completed":
            return "pending", "Waiting for required CI: " + name
        if newest.get("conclusion") != "success":
            return "blocked", "Required CI did not pass: " + name
    return "passed", "Required source, transport, and HA runtime checks passed"


def check(token, full_repo, number, sha):
    info = request(token, "/repos/" + full_repo + "/pulls/" + str(number))
    if info["head"]["sha"] != sha:
        return "changed", "PR head changed, check newest commit"
    if info.get("state") != "open":
        return "skipped", "PR is closed; no Codex review required"
    owner, name = full_repo.split("/", 1)
    # An unresolved P1/P2 or a P1/P2 on the *current head* always blocks,
    # even when Codex is currently over quota.
    unresolved = blockers(token, owner, name, number, sha)
    if unresolved:
        return "blocked", "Unresolved Codex P1/P2: " + "; ".join(unresolved[:8])
    state, reason = required_ci_state(token, full_repo, sha)
    if state != "passed":
        return state, reason
    prefix = "/repos/" + full_repo
    revs = pages(token, prefix + "/pulls/" + str(number) + "/reviews")
    summaries = pages(token, prefix + "/issues/" + str(number) + "/comments")
    if review_ready(revs, summaries, sha):
        return "passed", "Latest-head Codex review completed; zero P1/P2; all CI passed"
    return "pending", "Waiting for exact-head Codex review; quota never bypasses mandatory review"


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
    expires = time.monotonic() + 900
    while True:
        result, message = check(token, full_repo, pr["number"], pr["head"]["sha"])
        print(message, flush=True)
        if result in ("passed", "skipped"):
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
