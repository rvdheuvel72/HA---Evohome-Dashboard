"""CI policy: review-first, second final review and exact-head merge safety."""
import sys
import unittest
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
             patch.object(gate, "pages", return_value=[final, old]):
            state, _ = gate.check_final_review("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "pending")
        new = self.summary(updated="2026-10-24T10:31:00Z")
        with patch.object(gate, "check_ci", return_value=("passed", "OK")), \
             patch.object(gate, "check_review_only", return_value=("passed", "OK")), \
             patch.object(gate, "pages", return_value=[final, new]):
            state, _ = gate.check_final_review("token", self.REPO, 12, self.SHA)
            self.assertEqual(state, "passed")

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
