"""Keep Renovate majors explicit until HA runtime/toolchain compatibility is tested."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RenovateCompatibilityPolicyTests(unittest.TestCase):
    def test_major_upgrades_are_deferred_not_auto_merged(self):
        config = json.loads((ROOT / "renovate.json").read_text())
        self.assertFalse(config["automerge"])
        rules = config["packageRules"]
        evo = [rule for rule in rules
               if "evohome-async" in rule.get("matchPackageNames", [])]
        self.assertEqual(len(evo), 1)
        self.assertEqual(evo[0]["allowedVersions"], ">=2.1.0 <3.0.0")
        node = [rule for rule in rules
                if "node" in rule.get("matchDepNames", [])
                and "github-actions" in rule.get("matchManagers", [])]
        self.assertEqual(len(node), 1)
        self.assertEqual(node[0]["allowedVersions"], ">=24 <25")
        manifest = json.loads((ROOT / "custom_components" /
                               "evohome_dashboard" / "manifest.json").read_text())
        self.assertEqual(manifest["requirements"], ["evohome-async==2.1.0"])


if __name__ == "__main__":
    unittest.main()
