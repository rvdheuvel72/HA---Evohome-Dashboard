"""Stable HA release plan must never confuse firmware prereleases with HACS."""
from __future__ import annotations

import importlib.util
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
spec = importlib.util.spec_from_file_location(
    "plan_ha_release", ROOT / "scripts/plan_ha_release.py"
)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)


def release(tag, *, prerelease=False, draft=False):
    return {"tag_name": tag, "prerelease": prerelease, "draft": draft}


class StableReleasePlanTests(unittest.TestCase):
    def test_first_ha_release_ignores_esp32_beta_releases(self):
        releases = [
            release("ramses-arduino-v0.4.0-beta.37", prerelease=True),
            release("ramses-arduino-v0.4.0-beta.36", prerelease=True),
        ]
        self.assertEqual(mod.plan_release("0.3.5", releases), "publish")

    def test_current_ha_version_is_not_republished(self):
        self.assertEqual(
            mod.plan_release("0.3.5", [
                release("v0.3.5"),
                release("ramses-arduino-v0.4.0-beta.38", prerelease=True),
            ]),
            "already_published",
        )

    def test_new_version_publishes_only_after_previous_stable(self):
        self.assertEqual(
            mod.plan_release("0.3.6", [
                release("v0.3.5"), release("v0.3.4"),
                release("ramses-arduino-v0.4.0-beta.39", prerelease=True),
            ]),
            "publish",
        )

    def test_never_publish_older_version(self):
        with self.assertRaisesRegex(ValueError, "older HA version"):
            mod.plan_release("0.3.4", [release("v0.3.5")])

    def test_refuse_duplicate_or_prerelease_in_ha_namespace(self):
        with self.assertRaisesRegex(ValueError, "Duplicate stable"):
            mod.plan_release("0.3.6", [release("v0.3.5"), release("v0.3.5")])
        with self.assertRaisesRegex(ValueError, "cannot be draft/prerelease"):
            mod.plan_release("0.3.6", [release("v0.3.5", prerelease=True)])

    def test_exact_changelog_section_required(self):
        doc = (ROOT / "docs/HA_CHANGELOG.md").read_text()
        notes = mod.changelog_entry(doc, "0.3.5")
        self.assertIn("Automatic HACS update detection", notes)
        self.assertIn("not ESP32 firmware", notes)
        self.assertNotIn("## 0.3.4", notes)
        with self.assertRaisesRegex(ValueError, "Missing or ambiguous"):
            mod.changelog_entry(doc, "99.0.0")

    def test_manifest_matches_release_semver(self):
        manifest = json.loads(
            (ROOT / "custom_components/evohome_dashboard/manifest.json").read_text()
        )
        self.assertEqual(manifest["version"], "0.3.6")
        self.assertEqual(mod.plan_release(manifest["version"], []), "publish")


if __name__ == "__main__":
    unittest.main()
