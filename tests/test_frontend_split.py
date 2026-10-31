"""Frontend/backend separation must remain restart-free for UI-only changes."""
from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
COMP = ROOT / "custom_components" / "evohome_dashboard"
spec = importlib.util.spec_from_file_location(
    "frontend_packaging", ROOT / "scripts" / "package_frontend.py"
)
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


class FrontendSplitTests(unittest.TestCase):
    def test_backend_loader_is_small_and_uses_no_cache_hacs_resource(self):
        loader = (COMP / "frontend" / "dashboard.js").read_text(encoding="utf-8")
        self.assertLess(len(loader), 2200)
        self.assertIn(
            "/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js",
            loader,
        )
        self.assertIn("await import(BUNDLED_URL)", loader)
        self.assertIn("await import(FRONTEND_URL)", loader)
        self.assertIn('fetch(FRONTEND_URL, {cache: "no-store"})', loader)
        self.assertIn("if (externalCompatible)", loader)
        self.assertIn('new URL(import.meta.url).searchParams.get("v")', loader)
        self.assertIn("REQUIRED_FEATURE", loader)
        self.assertIn("backend-version-label-v1", loader)
        self.assertIn("REQUIRED_FEATURES.every", loader)
        ui = (ROOT / "frontend-package/dist/evohome-dashboard-ui.js").read_text()
        marker = "/* EVOHOME_UI_FEATURE: heating-demand-v1 */"
        self.assertIn(marker, loader)
        self.assertIn(marker, ui)
        self.assertIn("backend-version-label-v1", ui)
        self.assertIn('["demand","Heating demand"]', ui)
        bundled = (COMP / "frontend" / "dashboard-legacy.js").read_text()
        self.assertIn('["demand","Heating demand"]', bundled)

    def test_frontend_assets_registered_once_without_restart(self):
        init = (COMP / "__init__.py").read_text(encoding="utf-8")
        self.assertIn('assets / "dashboard.js"', init)
        self.assertIn('assets / "dashboard-legacy.js"', init)
        self.assertIn("cache_headers=False", init)
        self.assertIn("register_commands(hass)", init)
        self.assertIn("frontend.async_remove_panel", init)

    def test_legacy_fallback_preserves_custom_elements(self):
        legacy = (COMP / "frontend" / "dashboard-legacy.js").read_text()
        ui = (ROOT / "frontend-package/dist/evohome-dashboard-ui.js").read_text()
        for item in ('customElements.define("evohome-dashboard-panel"',
                     'customElements.define("evohome-dashboard-card"'):
            self.assertIn(item, ui)
            self.assertIn(item, legacy)

    def test_package_exact_frontend_files_and_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            filename = packaging.build(Path(directory) / "ui.zip")
            with ZipFile(filename) as archive:
                self.assertEqual(set(archive.namelist()), set(packaging.FILES))
                manifest = json.loads(archive.read("hacs.json"))
                self.assertEqual(manifest["filename"], "evohome-dashboard-ui.js")
                self.assertFalse(any(n.startswith("custom_components/") for n in archive.namelist()))

    def test_independent_versions(self):
        back = json.loads((COMP / "manifest.json").read_text())
        self.assertEqual(back["version"], "0.3.4")
        self.assertEqual(back["requirements"], ["evohome-async==2.1.0"])


if __name__ == "__main__":
    unittest.main()
