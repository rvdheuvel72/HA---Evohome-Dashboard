"""Verify HACS integration SemVer discipline matches the working Hyundai add-on gate."""
from __future__ import annotations

import importlib.util
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "check_ha_version.py"
spec = importlib.util.spec_from_file_location("check_ha_version", SCRIPT)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)


def manifest(version):
    return json.dumps({"domain": "evohome_dashboard", "version": version})


class HacsVersionGateTests(unittest.TestCase):
    def test_integration_runtime_changes_require_bump(self):
        with self.assertRaisesRegex(ValueError, "without manifest version bump"):
            mod.require_bump(
                manifest("0.3.4"), manifest("0.3.4"),
                ["custom_components/evohome_dashboard/sensor.py"],
            )
        self.assertEqual(
            mod.require_bump(
                manifest("0.3.4"), manifest("0.3.5"),
                ["custom_components/evohome_dashboard/frontend/dashboard-legacy.js"],
            ),
            "0.3.5",
        )

    def test_firmware_only_change_does_not_require_ha_bump(self):
        self.assertEqual(
            mod.require_bump(
                manifest("0.3.4"), manifest("0.3.4"),
                ["firmware/arduino-wroom32e/RamsesGateway/RamsesGateway.ino"],
            ),
            "0.3.4",
        )

    def test_independent_frontend_and_ci_only_changes_do_not_require_bump(self):
        self.assertEqual(
            mod.require_bump(
                manifest("0.3.5"), manifest("0.3.5"),
                ["frontend-package/dist/evohome-dashboard-ui.js",
                 ".github/workflows/publish-evohome-hacs.yml",
                 "scripts/check_ha_version.py", "tests/test_ha_release.py"],
            ),
            "0.3.5",
        )

    def test_version_must_not_decrease_even_without_runtime_changes(self):
        with self.assertRaisesRegex(ValueError, "must not decrease"):
            mod.require_bump(manifest("0.3.5"), manifest("0.3.4"), [])

    def test_ambiguous_or_prerelease_versions_are_rejected(self):
        for version in ("v0.3.5", "0.3.5-beta", "0.3.5.1", "0.03.5", "0.3", "latest"):
            with self.subTest(version=version):
                with self.assertRaises(ValueError):
                    mod.version_from_manifest(manifest(version))

    def test_github_workflow_checks_pr_version_and_only_publishes_after_ci(self):
        workflow = (ROOT / ".github/workflows/phase1-self-hosted.yml").read_text()
        release = (ROOT / ".github/workflows/publish-evohome-hacs.yml").read_text()
        self.assertIn("python3 scripts/check_ha_version.py --base-sha", workflow)
        self.assertIn("if: github.event_name == 'pull_request'", workflow)
        self.assertIn("workflow_run:", release)
        self.assertIn("github.event.workflow_run.conclusion == 'success'", release)
        self.assertIn("github.event.workflow_run.event == 'push'", release)
        self.assertIn("github.event.workflow_run.head_branch == 'main'", release)
        self.assertIn("contents: write", release)
        self.assertIn("gh release create", release)
        self.assertIn("--target \"$VERIFIED_SHA\"", release)
        self.assertIn("--latest", release)
        self.assertNotIn("workflow_dispatch:", release)
        self.assertNotIn("pull_request:", release)


if __name__ == "__main__":
    unittest.main()
