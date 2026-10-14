"""Fail-closed P1/P2 Codex review gate for latest GitHub PR head."""
from datetime import datetime, timedelta, timezone
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
    """Trust authenticated exact-head Codex completion summary.

    Codex may respond with a thumbs-up and updated summary instead of a
    Review object when it finds no issues. The P1/P2 blocker scan is a
    separate mandatory gate; old summaries are never accepted.
    """
    return any(
        is_codex(x.get("user"))
        and "codex-pull-request-review-summary" in x.get("body", "")
        and COMPLETED in x.get("body", "")
        and (chr(96) + sha[:7] + chr(96)) in x.get("body", "")
        for x in summaries
    )

CODEX_QUOTA_NOTICE = "You have reached your Codex usage limits for code reviews."
CODEX_QUOTA_BODY = (
    "You have reached your Codex usage limits for code reviews. "
    "You can see your limits in the "
    "[Codex usage dashboard](https://chatgpt.com/codex/cloud/settings/usage).\\n"
    "To continue using code reviews, you can upgrade your account or add "
    "credits to your account and enable them for code reviews in your "
    "[settings](https://chatgpt.com/codex/cloud/settings/code-review)."
)
QUOTA_GRACE = timedelta(hours=24)


def parse_utc(timestamp):
    if not isinstance(timestamp, str) or not timestamp:
        return None
    try:
        parsed = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
        return parsed.astimezone(timezone.utc) if parsed.tzinfo else None
    except ValueError:
        return None


def quota_deferred(comments, sha, now=None):
    """Accept a recent genuine PR quota notice plus exact-head request.

    The bot may stop responding to repeated review requests after quota
    exhaustion. Require a new, explicit full SHA request nevertheless.
    A quota notice is PR-specific and expires after 24h.
    """
    now = now or datetime.now(timezone.utc)
    requests = [
        parse_utc(c.get("created_at"))
        for c in comments
        if not is_codex(c.get("user"))
        and "@codex review" in c.get("body", "").lower()
        and sha in c.get("body", "")
    ]
    requests = [t for t in requests if t and t <= now]
    if not requests:
        return False
    latest_request = max(requests)
    if now-latest_request > QUOTA_GRACE:
        return False
    notices = [
        parse_utc(c.get("created_at"))
        for c in comments
        if is_codex(c.get("user"))
        and c.get("body", "").strip() == CODEX_QUOTA_BODY
    ]
    return any(t and t <= now and now-t <= QUOTA_GRACE for t in notices)


def review_disposition(reviews, comments, sha, now=None):
    if review_ready(reviews, comments, sha):
        return "reviewed"
    if quota_deferred(comments, sha, now=now):
        return "quota_deferred"
    return "missing"


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



REQUIRED_CHECKS = (
    "Python contract and dashboard syntax",
    "Independent HACS frontend package",
    "Pinned evohome-async runtime contract",
    "Home Assistant 2026.10 runtime imports",
)


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
    disposition = review_disposition(revs, summaries, sha)
    if disposition == "reviewed":
        return "passed", "Latest-head Codex review completed; zero P1/P2; all CI passed"
    if disposition == "quota_deferred":
        return "passed", (
            "Codex review deferred by verified quota notice for exact head "
            + sha + "; zero P1/P2; all CI passed; REVIEW DEBT must be tracked "
            "and revisited on the next available review run"
        )
    return "pending", "Waiting for exact-head Codex review or verified current-head quota notice"


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
