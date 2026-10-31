"""HA 2026.10 runtime tests for safe MQTT callbacks and fixed persistence deadlines."""
from __future__ import annotations

import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch

from homeassistant.core import is_callback

from custom_components.evohome_dashboard.rf_monitor import RfDemandMonitor
from custom_components.evohome_dashboard.heat_demand import HeatDemandLedger
from custom_components.evohome_dashboard.sensor import RFZoneDemand, RFZoneMinutes


class MonitorCallbackTests(unittest.TestCase):
    def test_ha_schedules_handlers_as_event_loop_callbacks(self):
        self.assertTrue(is_callback(RfDemandMonitor._on_message))
        self.assertTrue(is_callback(RfDemandMonitor._on_tick))
        self.assertTrue(is_callback(RfDemandMonitor._queue_checkpoint))
        self.assertTrue(is_callback(RfDemandMonitor._checkpoint_data))

    def test_sustained_rf_messages_do_not_debounce_forever(self):
        monitor = object.__new__(RfDemandMonitor)
        monitor.hass = SimpleNamespace()
        monitor.signal = "test_rf_signal"
        monitor.ledger = Mock()
        monitor.refresh_names = Mock()
        monitor.ledger.observe_mqtt.return_value = (True, 3)
        monitor.ledger.daily_storage.return_value = {"date": "2026-10-31", "zones": {}}
        monitor._store = Mock()
        monitor._checkpoint_pending = False
        with patch("custom_components.evohome_dashboard.rf_monitor.async_dispatcher_send") as send:
            for _ in range(100):
                monitor._on_message(SimpleNamespace(payload="sample"))
            self.assertEqual(monitor._store.async_delay_save.call_count, 1)
            self.assertEqual(send.call_count, 100)
            self.assertEqual(monitor.refresh_names.call_count, 100)
            self.assertTrue(monitor._checkpoint_pending)
            self.assertEqual(monitor._checkpoint_data()["date"], "2026-10-31")
            self.assertFalse(monitor._checkpoint_pending)
            monitor._on_message(SimpleNamespace(payload="sample"))
            self.assertEqual(monitor._store.async_delay_save.call_count, 2)

    def test_zone_sensor_name_tracks_late_cloud_match_and_rename(self):
        ledger = HeatDemandLedger("01:245914")
        monitor = SimpleNamespace(entry_id="example-entry", ledger=ledger)
        demand = RFZoneDemand(monitor, 0)
        duration = RFZoneMinutes(monitor, 0)
        original_unique_id = demand.unique_id
        self.assertEqual(demand.name, "RF zone 00 heat demand")
        ledger.zone_names[0] = "Woonkamer"
        self.assertEqual(demand.name, "Woonkamer heat demand")
        self.assertEqual(duration.name, "Woonkamer estimated heat request today")
        ledger.zone_names[0] = "Nieuwe woonkamer"
        self.assertEqual(demand.name, "Nieuwe woonkamer heat demand")
        self.assertEqual(demand.unique_id, original_unique_id)

    def test_refresh_cloud_mapping_notifies_existing_sensor_entities(self):
        ledger = HeatDemandLedger("01:245914")
        monitor = object.__new__(RfDemandMonitor)
        monitor.hass = SimpleNamespace()
        monitor.signal = "rf-test-signal"
        monitor.ledger = ledger
        monitor._manual_zone_names = {}
        monitor._cloud_links = {"00": {"system_id": "sys1", "zone_id": "z1"}}
        cloud = {"locations": [{"systems": [{
            "id": "sys1", "zones": [{"id": "z1", "name": "Woonkamer"}],
        }]}]}
        monitor._cloud_state = lambda: cloud
        with patch("custom_components.evohome_dashboard.rf_monitor.async_dispatcher_send") as send:
            monitor.refresh_names()
            self.assertEqual(ledger.zone_names[0], "Woonkamer")
            send.assert_called_once_with(monitor.hass, monitor.signal, None)
            monitor.refresh_names()
            self.assertEqual(send.call_count, 1)
            cloud["locations"][0]["systems"][0]["zones"][0]["name"] = "Salon"
            monitor.refresh_names()
            self.assertEqual(ledger.zone_names[0], "Salon")
            self.assertEqual(send.call_count, 2)

    def test_passive_mqtt_does_not_write_when_unrecognized(self):
        monitor = object.__new__(RfDemandMonitor)
        monitor.hass = SimpleNamespace()
        monitor.signal = "test_rf_signal"
        monitor.ledger = Mock()
        monitor.refresh_names = Mock()
        monitor.ledger.observe_mqtt.return_value = (False, None)
        monitor._store = Mock()
        monitor._checkpoint_pending = False
        with patch("custom_components.evohome_dashboard.rf_monitor.async_dispatcher_send") as send:
            monitor._on_message(SimpleNamespace(payload="unrelated"))
        monitor._store.async_delay_save.assert_not_called()
        monitor.refresh_names.assert_not_called()
        send.assert_not_called()


if __name__ == "__main__":
    unittest.main()
