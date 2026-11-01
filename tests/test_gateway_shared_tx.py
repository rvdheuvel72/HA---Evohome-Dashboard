"""Static contracts for HA admin proxy of gateway-owned ID25 TX method.

These don't replace live HA/gateway tests; they guard against accidental
restoration of per-request method overrides and credential storage.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMP = ROOT / "custom_components/evohome_dashboard"


class SharedTxMethodContracts(unittest.TestCase):
    def test_ha_uses_only_gateway_persisted_selection(self):
        proxy = (COMP / "gateway_diagnostics.py").read_text(encoding="utf-8")
        socket = (COMP / "websocket.py").read_text(encoding="utf-8")
        ui = (COMP / "frontend/dashboard-legacy.js").read_text(encoding="utf-8")
        loader = (COMP / "frontend/dashboard.js").read_text(encoding="utf-8")
        self.assertIn('METHOD_PATH = "/api/v1/opentherm/tx-method"', proxy)
        self.assertIn('action == "method_get"', proxy)
        self.assertIn('action == "method_set"', proxy)
        self.assertIn('headers={"X-Gateway-Config": "tx-method"}', proxy)
        self.assertIn('data={"confirm": "experimental-single-read-25"}', proxy)
        self.assertNotIn('"method": method},\n            headers={"X-Gateway-Config": "one-shot"}', proxy)
        self.assertIn('("status", "trigger", "method_get", "method_set")', socket)
        self.assertIn('default="fifo"', socket)
        self.assertIn('this.gatewayMethod = "fifo"', ui)
        self.assertIn('gw-method-save', ui)
        self.assertIn('gw-method-load', ui)
        self.assertIn('gateway-shared-tx-v1', ui)
        self.assertIn('gateway-shared-tx-v1', loader)

    def test_security_and_upgrade_guardrails_retained(self):
        proxy = (COMP / "gateway_diagnostics.py").read_text(encoding="utf-8")
        self.assertIn('validate_gateway_ip(host)', proxy)
        self.assertIn('aiohttp.BasicAuth(username, password)', proxy)
        self.assertIn('allow_redirects": False', proxy)
        self.assertIn('action in ("trigger", "method_set") and len(password) < 12', proxy)
        self.assertIn('Shared method requires ESP32 gateway firmware beta.38', proxy)


if __name__ == "__main__":
    unittest.main()
