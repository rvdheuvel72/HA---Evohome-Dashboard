"""Guard HA R8810A diagnostics auth, UI, release and credential boundaries."""
from __future__ import annotations
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
COMP=ROOT/"custom_components"/"evohome_dashboard"

class GatewayRFDiagnosticContracts(unittest.TestCase):
    def test_only_admin_websocket_can_start_read_or_download(self):
        ws=(COMP/"websocket.py").read_text()
        self.assertIn("if not connection.user.is_admin:",ws)
        for name in ("rf_diag_start","rf_diag_status","rf_diag_export",
                     "rf_diag_existing_capture"):
            self.assertIn(f'"{name}"',ws)
        self.assertIn('vol.Required("data_id"): vol.In([0, 18, 25, 117])',ws)
        self.assertIn('vol.Required("confirm"): str',ws)

    def test_gateway_secrets_never_enter_status_or_html(self):
        ws=(COMP/"websocket.py").read_text()
        self.assertIn('"r8810a_diag": (hub.rf_diag.snapshot()',ws)
        self.assertNotIn('CONF_RF_GATEWAY_PASSWORD',ws)
        for path in (
            COMP/"frontend/dashboard-legacy.js",
            ROOT/"frontend-package/dist/evohome-dashboard-ui.js",
        ):
            text=path.read_text()
            self.assertNotIn("rf_gateway_password",text)
            self.assertNotIn("gateway_password",text)
            self.assertIn('["rfdiag","R8810A test"]',text)
            self.assertIn('data-action="diag-existing"',text)

    def test_local_gateway_config_has_masked_password_and_safe_backend(self):
        flow=(COMP/"config_flow.py").read_text()
        self.assertIn("TextSelectorType.PASSWORD",flow)
        self.assertIn("valid_gateway_host(gateway_host)",flow)
        self.assertIn("**self.config_entry.options, **user_input",flow)
        gateway=(COMP/"r8810a_diagnostic.py").read_text()
        self.assertIn("allow_redirects=False",gateway)
        self.assertIn("MAX_CAPTURE_BYTES",gateway)
        self.assertIn("if confirm != CONFIRM:",gateway)
        self.assertIn("Only IDs 18, 25 and 117",gateway)
        self.assertIn('self.api.post(READ, {',gateway)
        self.assertIn('"one-read-own-r8810a-as-observed-controller"',gateway)

    def test_ha_integration_bumps_version_not_firmware(self):
        version=(COMP/"manifest.json").read_text()
        self.assertIn('"version": "0.3.7"',version)
        loader=(COMP/"frontend/dashboard.js").read_text()
        self.assertIn("r8810a-test-page-v1",loader)
        self.assertIn("r8810a-test-page-v1",
                      (ROOT/"frontend-package/dist/evohome-dashboard-ui.js").read_text())
        self.assertTrue((ROOT/"docs/R8810A_HA_TEST_PAGE.md").is_file())

if __name__=="__main__":
    unittest.main()
