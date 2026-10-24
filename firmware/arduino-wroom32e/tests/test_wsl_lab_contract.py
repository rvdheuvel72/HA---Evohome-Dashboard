"""Static safety regressions for manual, WSL-only ESP32 hardware actions."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
YAML = ROOT / ".github/workflows/ramses-local-hardware.yml"
LAB = ROOT / "firmware/arduino-wroom32e/tools/wsl_lab.py"


class WslHardwareSafety(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = YAML.read_text(encoding="utf-8")
        cls.lab = LAB.read_text(encoding="utf-8")

    def test_dispatch_only(self):
        head = self.workflow.split("\npermissions:", 1)[0]
        self.assertRegex(head, r"(?m)^  workflow_dispatch:$")
        self.assertNotRegex(head, r"(?m)^  (?:push|pull_request|pull_request_target):$")

    def test_hardware_job_exact_main_and_wsl_labels(self):
        match = re.search(
            r"(?m)^  hardware:\n"
            r"    if: ([^\n]+)\n"
            r"    runs-on: ([^\n]+)$",
            self.workflow,
        )
        self.assertIsNotNone(match)
        self.assertEqual(
            match.group(1),
            "github.event_name == 'workflow_dispatch' && github.ref == 'refs/heads/main'",
        )
        self.assertEqual(match.group(2), "[self-hosted, Linux, X64, wsl-ubuntu]")

    def test_flash_requires_verified_wsl_serial_identity(self):
        self.assertIn('os.getenv("LAB_CONFIRM_FLASH") != "FLASH-WSL-USB"', self.lab)
        self.assertIn('candidate.parent != base or not candidate.is_symlink()', self.lab)
        self.assertIn('if after != before:', self.lab)
        self.assertIn('os.getenv("LAB_BRANCH") != "refs/heads/main"', self.lab)
        self.assertIn('os.getenv("LAB_EVENT") != "workflow_dispatch"', self.lab)

    def test_official_cli_version_checksum(self):
        self.assertIn('VERSION = "1.5.1"', self.lab)
        self.assertIn(
            "28a8e119c498a25607821c36cb2dc49e8463941b261a0d99091baa7bc692dd2b",
            self.lab,
        )
        self.assertIn('if digest != CLI_SHA256:', self.lab)


if __name__ == "__main__":
    unittest.main()
