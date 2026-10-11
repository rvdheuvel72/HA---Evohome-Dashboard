"""Codex gate regression tests: fail closed for stale reviews and P1/P2."""
import importlib.util
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

    def test_outdated_review_rejected(self):
        rev = [{"user": BOT, "commit_id": "b"*40, "submitted_at": "2026", "state": "COMMENTED"}]
        summary = [{"user": BOT, "body": "<!-- codex-pull-request-review-summary --> \u2705 **Completed** " + chr(96)+SHA[:7]+chr(96)}]
        self.assertFalse(gate.review_ready(rev, summary, SHA))
        rev[0]["commit_id"] = SHA
        self.assertTrue(gate.review_ready(rev, summary, SHA))

    def test_unresolved_p2_blocks_merge(self):
        r = [{"user": BOT, "commit_id": SHA, "submitted_at": "2026", "state":"COMMENTED"}]
        s = [{"user": BOT, "body": "<!-- codex-pull-request-review-summary --> \u2705 **Completed** " + chr(96)+SHA[:7]+chr(96)}]
        with patch.object(gate,"request",return_value={"head":{"sha":SHA}}), \
             patch.object(gate,"pages",side_effect=[r,s]), \
             patch.object(gate,"blockers",return_value=["P2 expiry problem"]):
            state,_=gate.check("token","owner/repo",1,SHA)
        self.assertEqual(state,"blocked")

    def test_clean_review_passes(self):
        r = [{"user": BOT, "commit_id": SHA, "submitted_at": "2026", "state":"COMMENTED"}]
        s = [{"user": BOT, "body": "<!-- codex-pull-request-review-summary --> \u2705 **Completed** " + chr(96)+SHA[:7]+chr(96)}]
        with patch.object(gate,"request",return_value={"head":{"sha":SHA}}), \
             patch.object(gate,"pages",side_effect=[r,s]), \
             patch.object(gate,"blockers",return_value=[]):
            state,_=gate.check("token","owner/repo",1,SHA)
        self.assertEqual(state,"passed")
