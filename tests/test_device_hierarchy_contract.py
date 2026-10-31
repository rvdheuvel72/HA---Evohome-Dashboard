"""Regression contracts for HA 2026.8+ device registry parent IDs."""
from __future__ import annotations

import ast
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMP = ROOT / "custom_components" / "evohome_dashboard"


class DeviceHierarchyContracts(unittest.TestCase):
    def test_no_deprecated_via_device_identifiers(self):
        for platform in ("climate", "sensor", "switch", "button", "binary_sensor", "select"):
            with self.subTest(platform=platform):
                source = (COMP / f"{platform}.py").read_text()
                tree = ast.parse(source)
                keys = [
                    key.value for node in ast.walk(tree) if isinstance(node, ast.Dict)
                    for key in node.keys if isinstance(key, ast.Constant)
                ]
                self.assertNotIn("via_device", keys)
        for platform in ("climate", "sensor", "switch", "button"):
            with self.subTest(platform=platform):
                text = (COMP / f"{platform}.py").read_text()
                self.assertIn('"via_device_id": self.coordinator.system_device_ids[', text)

    def test_parent_created_before_platform_entities(self):
        init = (COMP / "__init__.py").read_text()
        self.assertIn("register_system_devices(hass, entry.entry_id, hub.client)", init)
        self.assertLess(init.index("hub.system_device_ids = register_system_devices("),
                        init.index("await hass.config_entries.async_forward_entry_setups("))
        helper = (COMP / "device_hierarchy.py").read_text()
        self.assertIn("registry.async_get_or_create(", helper)
        self.assertIn("config_entry_id=entry_id", helper)
        self.assertIn('identifiers={(DOMAIN, f"system_{system.id}")}', helper)
        self.assertIn("result[system.id] = device.id", helper)

    def test_zone_unique_ids_unchanged(self):
        # Preserve existing HA entities/device-area assignments after upgrade.
        climate = (COMP / "climate.py").read_text()
        sensor = (COMP / "sensor.py").read_text()
        self.assertIn('f"{DOMAIN}_{system_id}_{zone_id}"', climate)
        self.assertIn('f"zone_{self._zone_id}"', climate)
        self.assertIn('f"zone_{self.zone_id}"', sensor)


if __name__ == "__main__":
    unittest.main()
