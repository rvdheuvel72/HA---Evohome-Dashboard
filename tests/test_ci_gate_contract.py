"""Protect CI ordering: a Codex review must precede self-hosted builds."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOWS = ROOT / ".github" / "workflows"


class ReviewGateContract(unittest.TestCase):
    def test_expensive_jobs_depend_on_early_review(self):
        paths = {
            "phase1-self-hosted.yml": ("verify", "standalone-frontend-bundle",
                                         "pinned-evohome-api", "ha-runtime-import"),
            "ramses-arduino-wroom32e.yml": ("compile",),
            "trusted-ramses-compile.yml": ("verified-arduino",),
            "codex-p1-p2-review.yml": ("await-validation",),
        }
        for path, jobs in paths.items():
            with self.subTest(workflow=path):
                text = (WORKFLOWS / path).read_text()
                self.assertIn("  review-preflight:\n", text)
                self.assertIn("name: Initial Codex P1/P2 review", text)
                self.assertIn("runs-on: ubuntu-latest", text)
                self.assertIn("ref: main\n", text)  # Trusted script, never PR-supplied
                self.assertIn("from check_codex_review import blockers, pages, request, review_ready", text)
                self.assertIn("if review_ready(reviews, comments, sha):", text)
                self.assertIn("findings = blockers(token, owner, name, number, sha)", text)
                self.assertIn("name: Require authenticated exact-head Codex review before CI", text)
                self.assertLess(text.index("  review-preflight:\n"),
                                text.index("  " + jobs[0] + ":\n"))
                for job in jobs:
                    self.assertIn("  " + job + ":\n    needs: [review-preflight]\n", text)

    def test_feature_push_cannot_bypass_review(self):
        text = (WORKFLOWS / "ramses-arduino-wroom32e.yml").read_text()
        self.assertIn("  push:\\n    branches: [main]\\n", text)
        self.assertIn("  pull_request:\\n", text)

    def test_final_review_is_retained(self):
        text = (WORKFLOWS / "codex-p1-p2-review.yml").read_text()
        self.assertIn("  codex-review:\n", text)
        self.assertIn("needs: [await-validation]", text)
        self.assertIn("python3 scripts/check_codex_review.py\n", text)


if __name__ == "__main__":
    unittest.main()
