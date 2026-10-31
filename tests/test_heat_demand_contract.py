"""Contracts: RF observation is optional, authenticated and strictly passive."""
import unittest
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

    def test_dashboard_and_ha_sensor_have_demand_values(self):
        sensor = (HA / "sensor.py").read_text()
        websocket = (HA / "websocket.py").read_text()
        self.assertIn("class RFZoneDemand(", sensor)
        self.assertIn("class RFZoneMinutes(", sensor)
        self.assertIn('"heat_demand": (hub.rf_monitor.snapshot()', websocket)
        for path in (HA / "frontend" / "dashboard-legacy.js",
                     ROOT / "frontend-package" / "dist" / "evohome-dashboard-ui.js"):
            contents = path.read_text()
            self.assertIn('["demand","Heating demand"]', contents)
            self.assertIn("this.rfDemand=x.heat_demand||null", contents)
            self.assertIn("heatDemand() {", contents)

    def test_hub_cloud_writes_are_independent_of_rf_monitor(self):
        hub = (HA / "hub.py").read_text()
        self.assertNotIn("rf_monitor", hub)
        self.assertIn("async def set_zone(", hub)


if __name__ == "__main__":
    unittest.main()
