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

    def test_gateway_compiles_only_for_firmware_source_or_manual_release(self):
        manual = (WORKFLOWS / "ramses-arduino-wroom32e.yml").read_text()
        trusted = (WORKFLOWS / "trusted-ramses-compile.yml").read_text()
        publisher = (WORKFLOWS / "publish-ramses-arduino-v0.4-beta.yml").read_text()
        ha = (WORKFLOWS / "phase1-self-hosted.yml").read_text()
        for workflow in (manual, publisher):
            head = workflow.split("\npermissions:", 1)[0].split("\n# Superseded", 1)[0]
            self.assertIn("  workflow_dispatch:", head)
            self.assertNotIn("  push:", head)
            self.assertNotIn("  pull_request:", head)
        self.assertIn("  pull_request_target:", trusted)
        self.assertIn("firmware/arduino-wroom32e/RamsesGateway/**", trusted)
        self.assertIn("Require firmware README updates before provisioning Arduino", trusted)
        self.assertIn("git show \"$BASE_SHA:scripts/check_ha_docs.py\"", trusted)
        self.assertNotIn("firmware/arduino-wroom32e/**", trusted)
        # Root README maintenance must not trigger the HA runtime test matrix.
        self.assertNotIn("      - 'README.md'\n", ha)

    def test_final_review_is_retained(self):
        text = (WORKFLOWS / "codex-p1-p2-review.yml").read_text()
        self.assertIn("  codex-review:\n", text)
        self.assertIn("needs: [await-validation]", text)
        self.assertIn("python3 scripts/check_codex_review.py\n", text)


if __name__ == "__main__":
    unittest.main()
