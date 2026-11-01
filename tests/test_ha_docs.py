"""Unit tests for the documentation synchronization gate."""
from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("check_ha_docs", ROOT / "scripts" / "check_ha_docs.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

VERSION = "0.3.7"
README = "Current HA integration source version: v0.3.7"
CHANGELOG = "## 0.3.7\n\nGateway diagnostic added.\n"
GUIDE = "# Using Evohome Dashboard v0.3.7\n"


class DocumentationGateTests(unittest.TestCase):
    def test_docs_only_change_requires_no_bump(self):
        module.require_docs(["README.md"], VERSION, README, CHANGELOG, GUIDE)

    def test_runtime_requires_readme_and_changelog(self):
        changed = ["custom_components/evohome_dashboard/sensor.py"]
        with self.assertRaisesRegex(ValueError, "README"):
            module.require_docs(changed, VERSION, README, CHANGELOG, GUIDE)
        with self.assertRaisesRegex(ValueError, "HA_CHANGELOG"):
            module.require_docs(changed + ["README.md"], VERSION, README, CHANGELOG, GUIDE)

    def test_frontend_requires_guide(self):
        changed = ["custom_components/evohome_dashboard/frontend/dashboard.js",
                   "README.md", "docs/HA_CHANGELOG.md"]
        with self.assertRaisesRegex(ValueError, "USER_GUIDE"):
            module.require_docs(changed, VERSION, README, CHANGELOG, GUIDE)
        module.require_docs(changed + ["docs/USER_GUIDE.md"], VERSION, README, CHANGELOG, GUIDE)

    def test_version_must_match(self):
        changed = ["custom_components/evohome_dashboard/websocket.py", "README.md",
                   "docs/HA_CHANGELOG.md", "docs/USER_GUIDE.md"]
        with self.assertRaisesRegex(ValueError, "changelog"):
            module.require_docs(changed, VERSION, README, "## 0.3.6\n", GUIDE)
        with self.assertRaisesRegex(ValueError, "README"):
            module.require_docs(changed, VERSION, "v0.3.6", CHANGELOG, GUIDE)

    def test_firmware_only_does_not_require_ha_changelog(self):
        module.require_docs(["firmware/arduino-wroom32e/RamsesGateway/RamsesGateway.ino"],
                            VERSION, "", "", "")


if __name__ == "__main__":
    unittest.main()
