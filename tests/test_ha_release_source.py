"""Do not strand an HA release after unrelated firmware/documentation commits."""
from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "check_ha_release_source",
    ROOT / "scripts/check_ha_release_source.py",
)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)


class ReleaseSourceTests(unittest.TestCase):
    def test_firmware_only_commit_is_safe(self):
        self.assertEqual(mod.changed_critical([
            "firmware/arduino-wroom32e/RamsesGateway/RamsesGateway.ino",
            "docs/RAMSES_CONCURRENCY_AUDIT.md",
            "README.md",
        ]), [])

    def test_integration_change_must_wait_for_newer_ci(self):
        self.assertEqual(mod.changed_critical([
            "custom_components/evohome_dashboard/manifest.json",
            "custom_components/evohome_dashboard/heat_demand.py",
            "firmware/arduino-wroom32e/README.md",
        ]), [
            "custom_components/evohome_dashboard/heat_demand.py",
            "custom_components/evohome_dashboard/manifest.json",
        ])

    def test_release_critical_policy_changes_are_blocked(self):
        critical = [
            "hacs.json", "docs/HA_CHANGELOG.md",
            "scripts/check_codex_review.py",
            "scripts/check_ha_release_readiness.py",
            "scripts/check_ha_release_source.py",
            ".github/workflows/publish-evohome-hacs.yml",
            ".github/workflows/phase1-self-hosted.yml",
        ]
        # Source-check script is itself release-critical. This test would catch
        # any accidental omission when adding a new release file.
        self.assertEqual(mod.changed_critical(critical), sorted(critical))

    def test_workflow_allows_safe_later_main_commits(self):
        text = (ROOT / ".github/workflows/publish-evohome-hacs.yml").read_text()
        self.assertIn("compare/$VERIFIED_SHA...$current", text)
        self.assertIn("python3 scripts/check_ha_release_source.py", text)
        self.assertIn('if [ "$status" != "ahead" ]', text)
        ci = (ROOT / ".github/workflows/phase1-self-hosted.yml").read_text()
        self.assertIn("'docs/HA_CHANGELOG.md'", ci)


if __name__ == "__main__":
    unittest.main()
