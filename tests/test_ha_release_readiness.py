"""Test release provenance: direct main pushes and quota-unreviewed PRs fail closed."""
from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
spec = importlib.util.spec_from_file_location(
    "check_ha_release_readiness",
    ROOT / "scripts/check_ha_release_readiness.py",
)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

MERGED_SHA = "a" * 40
HEAD_SHA = "b" * 40
REPO = "rvdheuvel72/HA---Evohome-Dashboard"
PR = {
    "number": 83, "merged_at": "2026-10-31T15:00:00Z",
    "merge_commit_sha": MERGED_SHA, "base": {"ref": "main"},
    "head": {"sha": HEAD_SHA},
}


class HacsReleaseReadinessTests(unittest.TestCase):
    @staticmethod
    def fake_pages(_token, endpoint):
        if endpoint.endswith("/pulls"):
            return [PR]
        if endpoint.endswith("/reviews"):
            return [{"state": "COMMENTED", "commit_id": HEAD_SHA}]
        if endpoint.endswith("/comments"):
            return [{"body": "completed exact-head Codex review"}]
        raise ValueError(endpoint)

    def test_reviewed_merged_pr_is_required(self):
        with (patch.object(mod, "pages", side_effect=self.fake_pages),
              patch.object(mod, "request", return_value=PR),
              patch.object(mod, "blockers", return_value=[]),
              patch.object(mod, "required_ci_state", return_value=("passed", "all checks passed")),
              patch.object(mod, "review_ready", return_value=True)):
            self.assertEqual(mod.assert_release_ready("token", REPO, MERGED_SHA), 83)

    def test_unreviewed_direct_push_must_not_release(self):
        with patch.object(mod, "pages", return_value=[]):
            with self.assertRaisesRegex(ValueError, "reviewed PR"):
                mod.assert_release_ready("token", REPO, MERGED_SHA)

    def test_quota_deferred_review_is_not_sufficient(self):
        with (patch.object(mod, "pages", side_effect=self.fake_pages),
              patch.object(mod, "request", return_value=PR),
              patch.object(mod, "blockers", return_value=[]),
              patch.object(mod, "required_ci_state", return_value=("passed", "ok")),
              patch.object(mod, "review_ready", return_value=False)):
            with self.assertRaisesRegex(ValueError, "no completed clean exact-head"):
                mod.assert_release_ready("token", REPO, MERGED_SHA)

    def test_failed_required_ci_blocks_release(self):
        with (patch.object(mod, "pages", side_effect=self.fake_pages),
              patch.object(mod, "request", return_value=PR),
              patch.object(mod, "blockers", return_value=[]),
              patch.object(mod, "required_ci_state", return_value=("blocked", "runtime failed"))):
            with self.assertRaisesRegex(ValueError, "CI not fully green"):
                mod.assert_release_ready("token", REPO, MERGED_SHA)

    def test_unresolved_p1_p2_blocks_release(self):
        with (patch.object(mod, "pages", side_effect=self.fake_pages),
              patch.object(mod, "request", return_value=PR),
              patch.object(mod, "blockers", return_value=["P2: unresolved"])):
            with self.assertRaisesRegex(ValueError, "unresolved P1/P2"):
                mod.assert_release_ready("token", REPO, MERGED_SHA)

    def test_workflow_invokes_provenance_guard(self):
        workflow = (ROOT / ".github/workflows/publish-evohome-hacs.yml").read_text()
        self.assertIn("python3 scripts/check_ha_release_readiness.py", workflow)
        self.assertIn("github.event.workflow_run.event == 'push'", workflow)
        self.assertIn("github.event.workflow_run.head_repository.full_name == github.repository", workflow)
        self.assertIn("github.event.workflow_run.conclusion == 'success'", workflow)
        self.assertIn("gh api --paginate", workflow)
        self.assertIn("gh release create", workflow)


if __name__ == "__main__":
    unittest.main()
