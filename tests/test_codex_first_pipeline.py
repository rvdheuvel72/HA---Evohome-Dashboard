"""CI policy: review-first, second final review and exact-head merge safety."""
import sys
import unittest
from datetime import datetime, timezone
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import check_codex_review as gate
import manage_codex_request as requests
import auto_merge_codex as merge


class CodexFirstTests(unittest.TestCase):
    SHA = "a" * 40
    REPO = "example/evohome"
    PR = {
        "state": "open", "draft": False,
        "head": {"sha": SHA, "repo": {"full_name": REPO}},
        "base": {"ref": "main"},
    }

    def summary(self, updated="2026-10-24T10:20:00Z"):
        return {
            "user": {"login": "chatgpt-codex-connector[bot]"},
            "body": ("<!-- codex-pull-request-review-summary -->\n"
                     "✅ **Completed** `aaaaaaa`"),
            "updated_at": updated,
        }

    def test_review_first_prevents_build_when_pending(self):
        with patch.object(gate, "request", return_value=self.PR), \
             patch.object(gate, "blockers", return_value=[]), \
             patch.object(gate, "pages", return_value=[]):
            state, _ = gate.check_review_only("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "pending")

    def test_review_first_blocks_p1(self):
        with patch.object(gate, "request", return_value=self.PR), \
             patch.object(gate, "blockers", return_value=["P1: safety"]):
            state, _ = gate.check_review_only("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "blocked")

    def test_review_first_requires_real_completion_no_quota_bypass(self):
        with patch.object(gate, "request", return_value=self.PR), \
             patch.object(gate, "blockers", return_value=[]), \
             patch.object(gate, "pages", side_effect=[[], [self.summary()]]):
            state, _ = gate.check_review_only("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "passed")

    def test_post_ci_review_must_be_after_request(self):
        marker = "<!-- codex-final-review:" + self.SHA + " -->"
        final = {
            "user": {"login": "github-actions[bot]"},
            "created_at": "2026-10-24T10:30:00Z",
            "body": marker,
        }
        old = self.summary(updated="2026-10-24T10:20:00Z")
        with patch.object(gate, "check_ci", return_value=("passed", "OK")), \
             patch.object(gate, "check_review_only", return_value=("passed", "OK")), \
             patch.object(gate, "final_review_requested_at",
                          return_value=datetime(2026, 10, 24, 10, 30, tzinfo=timezone.utc)), \
             patch.object(gate, "pages", return_value=[final, old]):
            state, _ = gate.check_final_review("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "pending")
        new = self.summary(updated="2026-10-24T10:31:00Z")
        with patch.object(gate, "check_ci", return_value=("passed", "OK")), \
             patch.object(gate, "check_review_only", return_value=("passed", "OK")), \
             patch.object(gate, "final_review_requested_at",
                          return_value=datetime(2026, 10, 24, 10, 30, tzinfo=timezone.utc)), \
             patch.object(gate, "pages", return_value=[final, new]):
            state, _ = gate.check_final_review("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "passed")

    def test_forged_final_marker_is_rejected(self):
        marker = "<!-- codex-final-review:" + self.SHA + ":run=1234 -->"
        comment = {"user": {"login": "github-actions[bot]"},
                   "created_at": "2026-10-24T10:30:00Z",
                   "body": marker}
        fake_run = {"path": ".github/workflows/pr-owned.yml",
                    "event": "pull_request_target",
                    "repository": {"full_name": self.REPO}}
        with patch.object(gate, "request", return_value=fake_run):
            self.assertIsNone(gate.final_review_requested_at(
                "token", self.REPO, [comment], self.SHA))

    def test_real_protected_final_job_is_authenticated(self):
        marker = "<!-- codex-final-review:" + self.SHA + ":run=1234 -->"
        comment = {"user": {"login": "github-actions[bot]"},
                   "created_at": "2026-10-24T10:30:00Z",
                   "body": marker}
        trusted_run = {"path": ".github/workflows/codex-p1-p2-review.yml",
                       "event": "pull_request_target",
                       "repository": {"full_name": self.REPO}}
        trusted_jobs = {"total_count": 1,
            "jobs": [{"name": "Request second Codex review AFTER CI",
                      "conclusion": "success",
                      "started_at": "2026-10-24T10:29:55Z",
                      "completed_at": "2026-10-24T10:30:01Z"}]}
        with patch.object(gate, "request",
                          side_effect=[trusted_run, trusted_jobs]):
            self.assertEqual(
                gate.final_review_requested_at("token", self.REPO,
                                               [comment], self.SHA),
                datetime(2026, 10, 24, 10, 30, tzinfo=timezone.utc))

    def test_auto_merge_rejects_stale_head(self):
        changed = dict(self.PR)
        changed["head"] = {"sha": "b" * 40,
                           "repo": {"full_name": self.REPO}}
        with patch.object(merge, "request", return_value=changed):
            with self.assertRaisesRegex(RuntimeError, "head"):
                merge.validate("token", self.REPO, 12, self.SHA)

    def test_auto_merge_rejects_incomplete_final(self):
        with patch.object(merge, "request", return_value=self.PR), \
             patch.object(merge, "check_final_review", return_value=("pending", "Waiting")):
            with self.assertRaisesRegex(RuntimeError, "not green"):
                merge.validate("token", self.REPO, 12, self.SHA)

    def test_pre_request_idempotent(self):
        marker = "<!-- codex-pre-review:" + self.SHA + " -->"
        comments = [{
            "user": {"login": "github-actions[bot]"},
            "body": marker,
        }]
        with patch.object(requests, "request", return_value=self.PR) as post, \
             patch.object(requests, "pages", side_effect=[comments, []]):
            requests.ensure_request("token", self.REPO, 12, self.SHA, "pre")
            self.assertEqual(post.call_count, 1)


if __name__ == "__main__":
    unittest.main()
