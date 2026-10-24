"""Fail-closed P1/P2 Codex review gate for latest GitHub PR head."""
from datetime import datetime, timedelta, timezone
import json
import os
import re
import sys
import time
from urllib.request import Request, urlopen
from urllib.error import HTTPError

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
    "[Codex usage dashboard](https://chatgpt.com/codex/cloud/settings/usage).\n"
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


def arduino_validation_required(paths):
    """Also require a green ESP32 compile when a PR changes radio or its publisher."""
    return any(path.startswith("firmware/arduino-wroom32e/") or
               path in (".github/workflows/ramses-arduino-wroom32e.yml",
                        ".github/workflows/publish-ramses-arduino-v0.4-beta.yml")
               for path in paths)


HA_CHECK_PATHS = (
    "custom_components/", "frontend-package/", "tests/",
    "tests_integration/", "scripts/",
)
HA_CHECK_FILES = {
    "hacs.json", "manifest.json", ".github/workflows/phase1-self-hosted.yml",
}

def ha_validation_required(paths):
    """Match the path filters in phase1-self-hosted.yml."""
    return any(path.startswith(HA_CHECK_PATHS) or path in HA_CHECK_FILES
               for path in paths)


TRUSTED_ARDUINO_WORKFLOW = ".github/workflows/trusted-ramses-compile.yml"
TRUSTED_ARDUINO_JOB = "Trusted Arduino compile"


def trusted_arduino_provenance(token, full_repo, sha, check):
    """Reject spoofed compile checks originating from PR-controlled YAML."""
    if (check.get("name") != TRUSTED_ARDUINO_JOB or
            (check.get("app") or {}).get("slug") != "github-actions" or
            check.get("head_sha") != sha):
        return "blocked", "Untrusted Arduino check identity or commit SHA"
    details = check.get("details_url") or ""
    match = re.fullmatch(
        r"https://github[.]com/" + re.escape(full_repo) +
        r"/actions/runs/([0-9]+)/job/([0-9]+)", details)
    if not match:
        return "blocked", "Untrusted Arduino check URL"
    run_id, job_id = int(match.group(1)), int(match.group(2))
    if check.get("id") != job_id:
        return "blocked", "Arduino check/job ID mismatch"
    run = request(token, "/repos/" + full_repo + "/actions/runs/" + str(run_id))
    if (run.get("path") != TRUSTED_ARDUINO_WORKFLOW or
            run.get("event") != "pull_request_target" or
            run.get("head_sha") != sha or
            (run.get("repository") or {}).get("full_name") != full_repo):
        return "blocked", "Arduino check not issued by trusted protected workflow"
    return "passed", "Protected exact-head Arduino compile provenance verified"


def required_ci_state(token, full_repo, sha, pr_number=None):
    """Require all relevant source tests, and a radio compile for firmware PRs."""
    required = list(REQUIRED_CHECKS) if pr_number is None else []
    if pr_number is not None:
        changed = pages(token, "/repos/" + full_repo +
                        "/pulls/" + str(pr_number) + "/files")
        paths = [f.get("filename", "") for f in changed]
        if ha_validation_required(paths):
            required.extend(REQUIRED_CHECKS)
        if arduino_validation_required(paths):
            required.append("Trusted Arduino compile")
    response = request(
        token, "/repos/" + full_repo + "/commits/" + sha + "/check-runs?per_page=100"
    )
    checks = response.get("check_runs") if isinstance(response, dict) else None
    if not isinstance(checks, list):
        raise RuntimeError("Cannot inspect required CI check runs")
    for name in required:
        matching = [run for run in checks if run.get("name") == name]
        if not matching:
            return "pending", "Waiting for required CI: " + name
        newest = max(matching, key=lambda run: (run.get("started_at") or "", run.get("id") or 0))
        if name == "Trusted Arduino compile":
            provenance, explanation = trusted_arduino_provenance(
                token, full_repo, sha, newest)
            if provenance != "passed":
                return provenance, explanation
        if newest.get("status") != "completed":
            return "pending", "Waiting for required CI: " + name
        if newest.get("conclusion") != "success":
            return "blocked", "Required CI did not pass: " + name
    return "passed", "Required source, transport, HA runtime and trusted firmware checks passed"


def check_review_only(token, full_repo, number, sha):
    """Review FIRST: zero self-hosted compilation until exact-head Codex is green."""
    info = request(token, "/repos/" + full_repo + "/pulls/" + str(number))
    if info["head"]["sha"] != sha:
        return "changed", "PR head changed; review the new commit"
    if info.get("state") != "open":
        return "skipped", "PR closed"
    if info.get("draft"):
        return "blocked", "Draft PR is not ready for review"
    owner, name = full_repo.split("/", 1)
    findings = blockers(token, owner, name, number, sha)
    if findings:
        return "blocked", f"Codex has {len(findings)} blocking P1/P2 finding(s); self-hosted CI is intentionally skipped. See PR review threads."
    prefix = "/repos/" + full_repo
    reviews = pages(token, prefix + f"/pulls/{number}/reviews")
    comments = pages(token, prefix + f"/issues/{number}/comments")
    # NO quota bypass. The user requires an actual completed, green review.
    if review_disposition(reviews, comments, sha) == "reviewed":
        return "passed", "Codex FIRST gate: exact SHA reviewed and zero P1/P2"
    return "pending", "Waiting for pre-CI Codex review of " + sha


def final_review_requested_at(token, full_repo, comments, sha):
    """Authenticate a final review marker through the actual protected job.

    The github-actions[bot] author alone is NOT proof. A PR-owned workflow
    may post with the same identity. Verify the referenced workflow run,
    job name, source workflow and the comment's timestamp within that job.
    """
    prefix = "/repos/" + full_repo
    pattern = re.compile(r"<!-- codex-final-review:" + re.escape(sha) +
                         r":run=([0-9]+) -->")
    matches = []
    for comment in comments:
        if (comment.get("user") or {}).get("login", "").lower() not in (
                "github-actions[bot]", "github-actions"):
            continue
        match = pattern.search(comment.get("body", ""))
        created = parse_utc(comment.get("created_at"))
        if not match or not created:
            continue
        run_id = int(match.group(1))
        try:
            workflow = request(token, prefix + f"/actions/runs/{run_id}")
        except HTTPError as error:
            # A forged marker may point to a nonexistent run. Ignore it,
            # but fail closed on permission errors, rate limits or outages.
            if error.code == 404:
                continue
            raise
        if (workflow.get("path") != ".github/workflows/codex-p1-p2-review.yml"
                or workflow.get("event") not in ("pull_request_target", "issue_comment")
                or (workflow.get("repository") or {}).get("full_name") != full_repo):
            continue
        payload = request(token, prefix + f"/actions/runs/{run_id}/jobs?per_page=100")
        jobs = payload.get("jobs") if isinstance(payload, dict) else None
        if not isinstance(jobs, list) or payload.get("total_count", 0) > len(jobs):
            continue  # no partial or unauthenticated job evidence
        for job in jobs:
            if (job.get("name") != "Request second Codex review AFTER CI"
                    or job.get("conclusion") != "success"):
                continue
            began = parse_utc(job.get("started_at"))
            ended = parse_utc(job.get("completed_at"))
            # Accept only an actual comment created during that exact
            # privileged job (allow 2s for GitHub timestamp rounding).
            if began and ended and (
                    began - timedelta(seconds=2) <= created <= ended + timedelta(seconds=2)):
                matches.append(created)
    return max(matches, default=None)



def check_final_review(token, full_repo, number, sha):
    """A SECOND Codex review must complete after required CI passed."""
    state, message = check_ci(token, full_repo, number, sha)
    if state != "passed":
        return state, message
    state, message = check_review_only(token, full_repo, number, sha)
    if state != "passed":
        return state, message
    prefix = "/repos/" + full_repo
    comments = pages(token, prefix + f"/issues/{number}/comments")
    requested = final_review_requested_at(token, full_repo, comments, sha)
    if not requested:
        return "pending", "Waiting for an exact-head post-CI Codex review request"
    summaries = [
        x for x in comments
        if is_codex(x.get("user"))
        and "codex-pull-request-review-summary" in x.get("body", "")
        and (parse_utc(x.get("updated_at")) or datetime.min.replace(tzinfo=timezone.utc))
            > requested
    ]
    if review_ready([], summaries, sha):
        return "passed", "Final post-CI Codex review completed, exact SHA, no P1/P2"
    return "pending", "Awaiting second Codex review completed AFTER CI"


def check_ci(token, full_repo, number, sha):
    """First stage: only finish when every relevant build and test has passed."""
    info = request(token, "/repos/" + full_repo + "/pulls/" + str(number))
    if info["head"]["sha"] != sha:
        return "changed", "PR head changed, check newest commit"
    if info.get("state") != "open":
        return "skipped", "PR is closed; no CI gate required"
    return required_ci_state(token, full_repo, sha, pr_number=number)


def check(token, full_repo, number, sha):
    # The final review stage cannot start before the CI preflight has passed.
    state, reason = check_ci(token, full_repo, number, sha)
    if state != "passed":
        return state, reason
    owner, name = full_repo.split("/", 1)
    # An unresolved P1/P2 or a P1/P2 on the *current head* always blocks,
    # even when Codex is currently over quota.
    unresolved = blockers(token, owner, name, number, sha)
    if unresolved:
        return "blocked", "Unresolved Codex P1/P2: " + "; ".join(unresolved[:8])
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
    if len(sys.argv) > 2 or (len(sys.argv) == 2 and sys.argv[1] not in
                                ("--ci-only", "--review-only", "--final")):
        raise ValueError("Unknown gate option")
    mode = sys.argv[1] if len(sys.argv) == 2 else "--legacy"
    with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as stream:
        event = json.load(stream)
    token, full_repo = os.environ["GITHUB_TOKEN"], os.environ["GITHUB_REPOSITORY"]
    pr = event.get("pull_request")
    if pr is None and (event.get("issue") or {}).get("pull_request"):
        number = event["issue"]["number"]
        pr = request(token, "/repos/" + full_repo + "/pulls/" + str(number))
    if pr is None:
        raise RuntimeError("A PR event is required")
    expires = time.monotonic() + (1800 if mode in
                           ("--ci-only", "--review-only", "--final") else 900)
    while True:
        task = {"--ci-only": check_ci,
                "--review-only": check_review_only,
                "--final": check_final_review}.get(mode, check)
        result, message = task(token, full_repo, pr["number"], pr["head"]["sha"])
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
