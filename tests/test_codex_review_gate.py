"""Codex gate regression tests: fail closed for stale reviews and P1/P2."""
import importlib.util
from datetime import datetime, timedelta, timezone
from pathlib import Path
import unittest
from unittest.mock import patch

path = Path(__file__).resolve().parents[1] / "scripts/check_codex_review.py"
spec = importlib.util.spec_from_file_location("codex_gate", path)
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

SHA = "a" * 40
BOT = {"login": "chatgpt-codex-connector[bot]"}

class GateTests(unittest.TestCase):
    def test_only_exact_known_codex_identity(self):
        self.assertTrue(gate.is_codex({'login': 'chatgpt-codex-connector[bot]'}))
        self.assertTrue(gate.is_codex({'login': 'chatgpt-codex-connector'}))
        self.assertFalse(gate.is_codex({'login': 'chatgpt-codex-connector-attacker'}))

    def test_current_head_finding_cannot_be_resolved_away(self):
        current = {"body": "![P1 Badge] issue", "author": BOT, "originalCommit": {"oid": SHA}}
        old = dict(current, originalCommit={"oid": "b"*40})
        self.assertTrue(gate.finding_blocks({"isResolved": True}, current, SHA))
        self.assertTrue(gate.finding_blocks({"isResolved": False}, old, SHA))
        self.assertFalse(gate.finding_blocks({"isResolved": True}, old, SHA))

    def test_priority(self):
        self.assertTrue(gate.is_p12("![P2 Badge](https://img.shields.io/badge/P2-yellow)"))
        self.assertTrue(gate.is_p12("[P1] outage"))
        self.assertFalse(gate.is_p12("P3 only"))

    def test_stale_summary_rejected_and_latest_clean_summary_accepted(self):
        # Codex clean reviews may not create a separate GitHub Review object.
        stale = [{"user": BOT, "body": "<!-- codex-pull-request-review-summary --> \u2705 **Completed** " + chr(96)+"b"*7+chr(96)}]
        current = [{"user": BOT, "body": "<!-- codex-pull-request-review-summary --> \u2705 **Completed** " + chr(96)+SHA[:7]+chr(96)}]
        forged = [{"user": {"login": "attacker"}, "body": current[0]["body"]}]
        self.assertFalse(gate.review_ready([], stale, SHA))
        self.assertFalse(gate.review_ready([], forged, SHA))
        self.assertTrue(gate.review_ready([], current, SHA))

    def test_unresolved_p2_blocks_merge(self):
        r = [{"user": BOT, "commit_id": SHA, "submitted_at": "2026", "state":"COMMENTED"}]
        s = [{"user": BOT, "body": "<!-- codex-pull-request-review-summary --> \u2705 **Completed** " + chr(96)+SHA[:7]+chr(96)}]
        with patch.object(gate,"request",return_value={"head":{"sha":SHA}, "state":"open"}), \
             patch.object(gate,"pages",side_effect=[r,s]), \
             patch.object(gate,"blockers",return_value=["P2 expiry problem"]), \
             patch.object(gate,"required_ci_state",return_value=("passed","CI passed")):
            state,_=gate.check("token","owner/repo",1,SHA)
        self.assertEqual(state,"blocked")

    def test_clean_review_passes(self):
        r = [{"user": BOT, "commit_id": SHA, "submitted_at": "2026", "state":"COMMENTED"}]
        s = [{"user": BOT, "body": "<!-- codex-pull-request-review-summary --> \u2705 **Completed** " + chr(96)+SHA[:7]+chr(96)}]
        with patch.object(gate,"request",return_value={"head":{"sha":SHA}, "state":"open"}), \
             patch.object(gate,"pages",side_effect=[r,s]), \
             patch.object(gate,"blockers",return_value=[]), \
             patch.object(gate,"required_ci_state",return_value=("passed","CI passed")):
            state,_=gate.check("token","owner/repo",1,SHA)
        self.assertEqual(state,"passed")

    def test_required_ci_fail_closed_on_failure_missing_and_pending(self):
        def sample(name, state="completed", conclusion="success"):
            return {"name": name, "status": state, "conclusion": conclusion,
                    "started_at": "2026-10-11T10:00:00Z", "id": 1}
        success = [sample(n) for n in gate.REQUIRED_CHECKS]
        with patch.object(gate, "request", return_value={"check_runs": success}):
            self.assertEqual(gate.required_ci_state("token", "owner/repo", SHA)[0], "passed")
        for changed in (success[:-1], success[:-1] + [sample(
                gate.REQUIRED_CHECKS[-1], conclusion="failure")],
                success[:-1] + [sample(gate.REQUIRED_CHECKS[-1], state="in_progress")]):
            with patch.object(gate, "request", return_value={"check_runs": changed}):
                self.assertNotEqual(gate.required_ci_state("token", "owner/repo", SHA)[0], "passed")

    def test_verified_quota_debt_requires_exact_sha_and_bot_notice(self):
        now=datetime(2026,10,14,9,0,tzinfo=timezone.utc)
        stamp=lambda t: t.isoformat().replace("+00:00","Z")
        request={"user":{"login":"repo-owner"},
                 "body":"@codex review\\nExact head: "+SHA,
                 "created_at":stamp(now-timedelta(minutes=5))}
        bot={"user":BOT,"body":gate.CODEX_QUOTA_NOTICE,
             "created_at":stamp(now-timedelta(minutes=4))}
        self.assertTrue(gate.quota_deferred([request,bot], SHA, now))
        self.assertFalse(gate.quota_deferred([request,bot], "b"*40, now))
        self.assertFalse(gate.quota_deferred([bot,request], SHA,
                                            now+timedelta(hours=25)))
        self.assertFalse(gate.quota_deferred([request,dict(bot,user={"login":"impersonator"})],
                                             SHA,now))
        self.assertFalse(gate.quota_deferred([dict(request,created_at=stamp(now)),
                                              dict(bot,created_at=stamp(now-timedelta(minutes=4)))],
                                             SHA,now))
        self.assertEqual(gate.review_disposition([], [request,bot], SHA, now),
                         "quota_deferred")

    def test_quota_exception_with_green_ci_and_no_p12(self):
        now=datetime.now(timezone.utc)
        stamp=lambda t:t.isoformat().replace("+00:00","Z")
        comments=[{"user":{"login":"owner"},"body":"@codex review for "+SHA,
                   "created_at":stamp(now-timedelta(minutes=2))},
                  {"user":BOT,"body":gate.CODEX_QUOTA_NOTICE,
                   "created_at":stamp(now-timedelta(minutes=1))}]
        with patch.object(gate,"request",return_value={"head":{"sha":SHA},"state":"open"}), \
             patch.object(gate,"pages",side_effect=[[],comments]), \
             patch.object(gate,"blockers",return_value=[]), \
             patch.object(gate,"required_ci_state",return_value=("passed","CI green")):
            state,msg=gate.check("token","owner/repo",7,SHA)
        self.assertEqual(state,"passed")
        self.assertIn("REVIEW DEBT",msg)

    def test_quota_never_bypasses_ci_or_p12(self):
        with patch.object(gate,"request",return_value={"head":{"sha":SHA},"state":"open"}), \
             patch.object(gate,"blockers",return_value=["P1 existing issue"]), \
             patch.object(gate,"required_ci_state",return_value=("passed","CI green")):
            state,_=gate.check("token","owner/repo",7,SHA)
        self.assertEqual(state,"blocked")
        with patch.object(gate,"request",return_value={"head":{"sha":SHA},"state":"open"}), \
             patch.object(gate,"blockers",return_value=[]), \
             patch.object(gate,"required_ci_state",return_value=("blocked","CI failed")):
            state,_=gate.check("token","owner/repo",7,SHA)
        self.assertEqual(state,"blocked")

    def test_closed_pr_is_noop(self):
        with patch.object(gate,"request",return_value={"head":{"sha":SHA}, "state":"closed"}):
            state,_=gate.check("token","owner/repo",7,SHA)
        self.assertEqual(state,"skipped")
