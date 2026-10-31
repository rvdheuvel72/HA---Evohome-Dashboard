"""Frontend resource cache version must follow the integration manifest."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTEGRATION = ROOT / "custom_components" / "evohome_dashboard"


class FrontendVersionContractTest(unittest.TestCase):
    def test_dashboard_displays_current_backend_and_actual_ui_source(self):
        init = (INTEGRATION / "__init__.py").read_text()
        websocket = (INTEGRATION / "websocket.py").read_text()
        self.assertIn('hass.data[f"{DOMAIN}_backend_version"] = frontend_version', init)
        self.assertIn('"backend_version": hass.data.get(f"{DOMAIN}_backend_version")', websocket)
        for relative, source in (
            ("frontend/dashboard-legacy.js", "Bundled"),
            ("../../frontend-package/dist/evohome-dashboard-ui.js", "HACS"),
        ):
            with self.subTest(source=source):
                js = (INTEGRATION / relative).resolve().read_text()
                self.assertIn(f'const EVOHOME_UI_SOURCE = "{source}";', js)
                self.assertIn("this.backendVersion=x.backend_version||null", js)
                self.assertIn('this.backendVersion?"v"+this.backendVersion:"version unknown"', js)
                self.assertIn("· UI: '+EVOHOME_UI_SOURCE", js)

    def test_manifest_drives_dashboard_module_url(self):
        version = json.loads((INTEGRATION / "manifest.json").read_text())["version"]
        self.assertRegex(version, r"^\d+\.\d+\.\d+")
        source = (INTEGRATION / "__init__.py").read_text()
        self.assertIn('manifest_path = Path(__file__).with_name("manifest.json")', source)
        self.assertIn('frontend_version = json.loads(manifest_text)["version"]', source)
        self.assertIn('module_url=f"/api/{DOMAIN}/dashboard.js?v={frontend_version}"', source)
        self.assertIsNone(re.search(r'module_url=.*dashboard\.js\?v=\d', source))


if __name__ == "__main__":
    unittest.main()
