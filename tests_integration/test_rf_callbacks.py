"""HA 2026.10 runtime tests for safe MQTT callbacks and fixed persistence deadlines."""
from __future__ import annotations

import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch

from homeassistant.core import is_callback

from custom_components.evohome_dashboard.rf_monitor import RfDemandMonitor


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
        monitor.ledger.observe_mqtt.return_value = (True, 3)
        monitor.ledger.daily_storage.return_value = {"date": "2026-10-31", "zones": {}}
        monitor._store = Mock()
        monitor._checkpoint_pending = False
        with patch("custom_components.evohome_dashboard.rf_monitor.async_dispatcher_send") as send:
            for _ in range(100):
                monitor._on_message(SimpleNamespace(payload="sample"))
            self.assertEqual(monitor._store.async_delay_save.call_count, 1)
            self.assertEqual(send.call_count, 100)
            self.assertTrue(monitor._checkpoint_pending)
            self.assertEqual(monitor._checkpoint_data()["date"], "2026-10-31")
            self.assertFalse(monitor._checkpoint_pending)
            monitor._on_message(SimpleNamespace(payload="sample"))
            self.assertEqual(monitor._store.async_delay_save.call_count, 2)

    def test_passive_mqtt_does_not_write_when_unrecognized(self):
        monitor = object.__new__(RfDemandMonitor)
        monitor.hass = SimpleNamespace()
        monitor.signal = "test_rf_signal"
        monitor.ledger = Mock()
        monitor.ledger.observe_mqtt.return_value = (False, None)
        monitor._store = Mock()
        monitor._checkpoint_pending = False
        with patch("custom_components.evohome_dashboard.rf_monitor.async_dispatcher_send") as send:
            monitor._on_message(SimpleNamespace(payload="unrelated"))
        monitor._store.async_delay_save.assert_not_called()
        send.assert_not_called()


if __name__ == "__main__":
    unittest.main()
