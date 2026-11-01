"""Contracts: RF observation is optional, authenticated and strictly passive."""
import unittest
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HA = ROOT / "custom_components" / "evohome_dashboard"


class RfFeatureContracts(unittest.TestCase):
    def test_rf_transport_never_publishes_or_transmits(self):
        monitor = (HA / "rf_monitor.py").read_text()
        self.assertIn("mqtt.async_subscribe", monitor)
        self.assertNotIn("mqtt.publish(", monitor)
        self.assertNotIn("async_publish(", monitor)
        self.assertNotIn("send_frame(", monitor)
        self.assertIn("async_delay_save", monitor)
        self.assertIn("self._unsubscribe()", monitor)

    def test_rf_is_opt_in_and_configured_with_room_indices(self):
        init = (HA / "__init__.py").read_text()
        flow = (HA / "config_flow.py").read_text()
        self.assertIn("if controller_id:", init)
        self.assertIn("valid_zone_names(", init)
        self.assertIn("valid_controller_id(controller)", flow)
        self.assertIn("valid_zone_names(names)", flow)
        self.assertIn("await hub.rf_monitor.async_stop()", init)
        self.assertIn("if controller_id:\n            # Import optional MQTT", init)
        manifest = json.loads((HA / "manifest.json").read_text())
        self.assertEqual(manifest.get("after_dependencies"), ["mqtt"])

    def test_dashboard_and_ha_sensor_have_demand_values(self):
        sensor = (HA / "sensor.py").read_text()
        websocket = (HA / "websocket.py").read_text()
        self.assertIn("class RFZoneDemand(", sensor)
        self.assertIn("class RFZoneMinutes(", sensor)
        self.assertIn('"heat_demand": (hub.rf_monitor.snapshot()', websocket)
        self.assertIn("from homeassistant.core import callback", sensor)
        self.assertIn("@callback\n        def on_rf_event", sensor)
        self.assertIn("@callback\n        def on_update", sensor)
        for path in (HA / "frontend" / "dashboard-legacy.js",
                     ROOT / "frontend-package" / "dist" / "evohome-dashboard-ui.js"):
            contents = path.read_text()
            self.assertIn('["demand","Heating demand"]', contents)
            self.assertIn("this.rfDemand=x.heat_demand||null", contents)
            self.assertIn("heatDemand() {", contents)

    def test_hub_cloud_writes_are_independent_of_rf_monitor(self):
        import ast
        source = (HA / "hub.py").read_text()
        tree = ast.parse(source)
        hub_class = next(node for node in tree.body
                         if isinstance(node, ast.ClassDef) and node.name == "EvohomeHub")
        control = ("set_zone", "set_zone_frost", "resume_zone",
                   "set_system_mode", "set_hotwater", "resume_hotwater",
                   "save_zone_schedule", "_execute", "_wait_for_state")
        for method in (node for node in hub_class.body
                       if isinstance(node, ast.AsyncFunctionDef) and node.name in control):
            with self.subTest(method=method.name):
                segment = ast.get_source_segment(source, method)
                self.assertNotIn("rf_monitor", segment)
                self.assertNotIn("refresh_names", segment)
        self.assertIn("async def set_zone(", source)
        # After cloud status has been obtained, name-decoration failures must
        # never turn that successful cloud poll into UpdateFailed.
        update = next(node for node in hub_class.body
                      if isinstance(node, ast.AsyncFunctionDef)
                      and node.name == "_async_update_data")
        calls = [node for node in ast.walk(update)
                 if isinstance(node, ast.Call)
                 and isinstance(node.func, ast.Attribute)
                 and node.func.attr == "refresh_names"]
        self.assertEqual(len(calls), 1)
        self.assertTrue(any(isinstance(node, ast.Try)
                            and node.handlers and any(calls[0] in ast.walk(node)
                                                      for node in node.body)
                            for node in ast.walk(update)))
        self.assertIn("Never turn a", source)


if __name__ == "__main__":
    unittest.main()
