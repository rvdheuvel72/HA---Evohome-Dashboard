"""Contract: Renovate must not upgrade the HA runtime to unvalidated Python 3.15."""
import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RENOVATE = ROOT / "renovate.json"
WORKFLOW = ROOT / ".github/workflows/phase1-self-hosted.yml"


class TestPythonRuntimePolicy(unittest.TestCase):
    def test_ha_runtime_remains_pinned_to_supported_python_line(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        versions = re.findall(r"""python-version:\s*['"]?(\d+\.\d+)""", workflow)
        self.assertEqual(
            versions, ["3.14", "3.14"],
            "HA 2026.10 and pinned evohome-async were tested with Python 3.14, "
            "not 3.15. Upgrade intentionally with dependency validation.",
        )

    def test_renovate_excludes_python_315_but_allows_314_updates(self):
        config = json.loads(RENOVATE.read_text(encoding="utf-8"))
        rules = [
            r for r in config.get("packageRules", [])
            if "github-actions" in r.get("matchManagers", [])
            and "python" in r.get("matchDepNames", [])
        ]
        self.assertEqual(len(rules), 1, "Expected one Python toolchain policy")
        self.assertEqual(rules[0].get("allowedVersions"), ">=3.14 <3.15")
        self.assertIs(config.get("automerge"), False)


if __name__ == "__main__":
    unittest.main()
