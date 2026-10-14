import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import check_release_readiness as gate


SHA = "a" * 40
HEAD = "b" * 40
REPO = "rvdheuvel72/HA---Evohome-Dashboard"


def pr(merge_sha=SHA, base="main", merged="2026-10-14T09:00:00Z"):
    return {
        "number": 42,
        "merged_at": merged,
        "merge_commit_sha": merge_sha,
        "base": {"ref": base},
        "head": {"sha": HEAD},
    }


class CheckReleaseReadinessTests(unittest.TestCase):
    def test_matching_merged_pr_exact_commit_only(self):
        matches = gate.matching_merged_prs(
            [pr(), pr(merge_sha="c" * 40), pr(base="staging"), pr(merged=None)],
            SHA,
        )
        self.assertEqual(len(matches), 1)

    def test_no_associated_reviewed_pr_blocks(self):
        with patch.object(gate, "pages", return_value=[]):
            with self.assertRaisesRegex(RuntimeError, "not an approved PR"):
                gate.verify("secret", REPO, SHA)

    @patch.object(gate, "review_ready", return_value=True)
    @patch.object(gate, "required_ci_state", return_value=("passed", "passed"))
    @patch.object(gate, "blockers", return_value=[])
    @patch.object(gate, "request")
    @patch.object(gate, "pages")
    def test_verified_release_passes(self, pages, request, blockers, ci, review):
        pages.side_effect = lambda token, path: ([pr()] if "/commits/" in path else [])
        request.return_value = pr()
        self.assertEqual(gate.verify("secret", REPO, SHA), [42])
        self.assertTrue(review.called)

    @patch.object(gate, "required_ci_state", return_value=("passed", "passed"))
    @patch.object(gate, "blockers", return_value=[])
    @patch.object(gate, "request")
    @patch.object(gate, "pages")
    def test_missing_codex_review_blocks(self, pages, request, blockers, ci):
        pages.side_effect = lambda token, path: ([pr()] if "/commits/" in path else [])
        request.return_value = pr()
        with patch.object(gate, "review_ready", return_value=False):
            with self.assertRaisesRegex(RuntimeError, "lacks completed Codex"):
                gate.verify("secret", REPO, SHA)

    @patch.object(gate, "blockers", return_value=["P1 blocker"])
    @patch.object(gate, "request")
    @patch.object(gate, "pages")
    def test_unresolved_p1_blocks(self, pages, request, blockers):
        pages.side_effect = lambda token, path: ([pr()] if "/commits/" in path else [])
        request.return_value = pr()
        with self.assertRaisesRegex(RuntimeError, "unaddressed P1/P2"):
            gate.verify("secret", REPO, SHA)

    @patch.object(gate, "required_ci_state", return_value=("pending", "No CI"))
    @patch.object(gate, "blockers", return_value=[])
    @patch.object(gate, "request")
    @patch.object(gate, "pages")
    def test_pending_ci_blocks(self, pages, request, blockers, ci):
        pages.side_effect = lambda token, path: ([pr()] if "/commits/" in path else [])
        request.return_value = pr()
        with self.assertRaisesRegex(RuntimeError, "required CI not passed"):
            gate.verify("secret", REPO, SHA)


if __name__ == "__main__":
    unittest.main()
