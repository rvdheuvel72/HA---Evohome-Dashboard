"""Home Assistant runtime test: parent registry IDs are scoped per config entry."""
from types import SimpleNamespace
from unittest.mock import MagicMock, patch
import unittest

from custom_components.evohome_dashboard.device_hierarchy import register_system_devices


class ControllerDeviceRegistryTests(unittest.TestCase):
    def test_registers_parents_and_returns_actual_registry_ids(self):
        system1 = SimpleNamespace(id="sys-A", model="Evohome Wi-Fi")
        system2 = SimpleNamespace(id="sys-B", model="Evohome Wi-Fi")
        client = SimpleNamespace(locations=[
            SimpleNamespace(gateways=[
                SimpleNamespace(systems=[system1, system2])
            ])
        ])
        registry = MagicMock()
        registry.async_get_or_create.side_effect = [
            SimpleNamespace(id="device-registry-123"),
            SimpleNamespace(id="device-registry-456"),
        ]
        with patch(
            "custom_components.evohome_dashboard.device_hierarchy.dr.async_get",
            return_value=registry,
        ) as get_registry:
            device_ids = register_system_devices(object(), "entry-12", client)
        self.assertEqual(device_ids, {
            "sys-A": "device-registry-123",
            "sys-B": "device-registry-456",
        })
        get_registry.assert_called_once()
        self.assertEqual(registry.async_get_or_create.call_count, 2)
        first_call = registry.async_get_or_create.call_args_list[0].kwargs
        self.assertEqual(first_call["config_entry_id"], "entry-12")
        self.assertEqual(
            first_call["identifiers"],
            {("evohome_dashboard", "system_sys-A")},
        )
        self.assertNotIn("via_device", first_call)

    def test_no_systems_never_creates_a_parent(self):
        registry = MagicMock()
        with patch(
            "custom_components.evohome_dashboard.device_hierarchy.dr.async_get",
            return_value=registry,
        ):
            result = register_system_devices(
                object(), "entry-12", SimpleNamespace(locations=[])
            )
        self.assertEqual(result, {})
        registry.async_get_or_create.assert_not_called()


if __name__ == "__main__":
    unittest.main()
