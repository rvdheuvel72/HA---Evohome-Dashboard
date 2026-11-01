"""Cloud names are known, but RF indexes need RF 0004 evidence or a verified link."""
from __future__ import annotations

import importlib.util
import sys
import unittest
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMP = ROOT / "custom_components/evohome_dashboard"


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, COMP / file)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


links = load("evohome_test_room_links", "room_links.py")
heat = load("evohome_test_heat_demand_0004", "heat_demand.py")

DATA = {"locations": [{
    "id": "location-1", "name": "Home",
    "systems": [{
        "id": "system-1", "zones": [
            {"id": "cloud-100", "name": "Woonkamer"},
            {"id": "cloud-200", "name": "Darius"},
            {"id": "cloud-300", "name": "Benjamin"},
            {"id": "cloud-400", "name": "Kantoor"},
        ],
    }],
}]}
CTL = "01:245914"
AT = datetime(2026, 10, 31, 14, tzinfo=timezone.utc)


def radio_name_frame(index, name):
    encoded = name.encode("ascii")
    assert len(encoded) <= 20
    payload = bytes([index, 0]) + encoded.ljust(20, b"\x00")
    return heat.parse_frame(
        "045  I --- " + CTL + " --:------ " + CTL
        + " 0004 022 " + payload.hex().upper()
    )


class CloudRoomLinkTests(unittest.TestCase):
    def test_passive_controller_0004_zone_name_matches_known_cloud_name(self):
        ledger = heat.HeatDemandLedger(CTL)
        self.assertTrue(ledger.observe(radio_name_frame(0, "Woonkamer"), AT))
        self.assertEqual(ledger.radio_names, {0: "Woonkamer"})
        names, mapping = links.resolve_names(DATA, {}, {}, ledger.radio_names)
        self.assertEqual(names, {0: "Woonkamer"})
        self.assertEqual(mapping[0]["source"], "rf_0004")
        self.assertEqual(mapping[0]["zone_id"], "cloud-100")
        self.assertTrue(ledger.recent[-1]["kind"] == "rf_zone_name")

    def test_controller_0004_is_rejected_when_malformed_or_not_controller(self):
        ledger = heat.HeatDemandLedger(CTL)
        packet = radio_name_frame(0, "Woonkamer")
        self.assertTrue(ledger.observe(packet, AT))
        malformed = heat.parse_frame(
            "045  I --- " + CTL + " --:------ " + CTL +
            " 0004 022 00FF" + ("00" * 20)
        )
        self.assertFalse(ledger.observe(malformed, AT))
        foreign = heat.parse_frame(
            "045  I --- 01:111111 --:------ 01:111111 0004 022 "
            + (bytes([0, 0]) + b"Woonkamer".ljust(20, b"\x00")).hex().upper()
        )
        self.assertFalse(ledger.observe(foreign, AT))
        self.assertEqual(ledger.radio_names, {0: "Woonkamer"})

    def test_cloud_list_order_or_cloud_zone_id_never_guessed(self):
        names, info = links.resolve_names(DATA, {}, {}, {})
        self.assertEqual(names, {})
        self.assertEqual(info, {})
        self.assertNotEqual(DATA["locations"][0]["systems"][0]["zones"][0]["id"], "00")

    def test_duplicate_names_do_not_auto_link(self):
        duplicate = {"locations": [{"systems": [{
            "id": "system-1",
            "zones": [{"id": "z1", "name": "Kantoor"}, {"id": "z2", "name": "Kantoor"}],
        }]}]}
        names, info = links.resolve_names(duplicate, {}, {}, {2: "Kantoor"})
        self.assertNotIn(2, names)
        self.assertEqual(info[2]["source"], "ambiguous")

    def test_user_selects_cloud_zone_and_future_name_updates_automatically(self):
        saved = links.set_link({}, "00", "system-1", "cloud-100", DATA)
        self.assertEqual(saved["00"], {"system_id": "system-1", "zone_id": "cloud-100"})
        names, info = links.resolve_names(DATA, saved, {}, {})
        self.assertEqual(names[0], "Woonkamer")
        self.assertEqual(info[0]["source"], "selected")
        renamed = {"locations": [{"systems": [{
            "id": "system-1", "zones": [{"id": "cloud-100", "name": "Nieuwe woonkamer"}],
        }]}]}
        names, info = links.resolve_names(renamed, saved, {}, {})
        self.assertEqual(names[0], "Nieuwe woonkamer")
        self.assertEqual(info[0]["zone_id"], "cloud-100")

    def test_existing_manual_json_names_are_preserved_unless_cloud_selected(self):
        names, info = links.resolve_names(DATA, {}, {0: "My manually named room"}, {0: "Woonkamer"})
        self.assertEqual(names[0], "My manually named room")
        self.assertEqual(info[0]["source"], "manual")
        saved = links.set_link({}, "00", "system-1", "cloud-100", DATA)
        names, info = links.resolve_names(DATA, saved, {0: "Old name"}, {})
        self.assertEqual(names[0], "Woonkamer")
        self.assertEqual(info[0]["source"], "selected")

    def test_numeric_cloud_zone_ids_are_normalized_to_stable_strings(self):
        numeric = {"locations": [{"systems": [{
            "id": 12345, "zones": [{"id": 67890, "name": "Woonkamer"}],
        }]}]}
        rows = links.cloud_zones(numeric)
        self.assertEqual(rows[0]["system_id"], "12345")
        self.assertEqual(rows[0]["zone_id"], "67890")
        saved = links.set_link({}, "00", "12345", "67890", numeric)
        self.assertEqual(
            links.resolve_names(numeric, saved, {}, {})[0][0],
            "Woonkamer"
        )

    def test_configure_does_not_erase_separately_saved_cloud_links(self):
        # Integration options form intentionally preserves options which its
        # fields do not display. The WebSocket persists these links separately.
        form = (COMP / "config_flow.py").read_text()
        self.assertIn("**self.config_entry.options, **user_input", form)
        self.assertIn("CONF_RF_ZONE_LINKS: links", (COMP / "websocket.py").read_text())

    def test_reject_missing_or_duplicate_cloud_room_links(self):
        with self.assertRaisesRegex(ValueError, "not a configured"):
            links.set_link({}, "00", "system-1", "does-not-exist", DATA)
        saved = links.set_link({}, "00", "system-1", "cloud-100", DATA)
        with self.assertRaisesRegex(ValueError, "already linked"):
            links.set_link(saved, "01", "system-1", "cloud-100", DATA)
        self.assertEqual(links.set_link(saved, "00", "", "", DATA), {})
        with self.assertRaises(ValueError):
            links.set_link({}, "0F", "system-1", "cloud-100", DATA)

    def test_radio_name_persisted_without_restoring_stale_demand(self):
        ledger = heat.HeatDemandLedger(CTL)
        ledger.observe(radio_name_frame(0, "Woonkamer"), AT)
        stored = ledger.daily_storage(AT)
        restored = heat.HeatDemandLedger(CTL)
        restored.restore_daily(stored, AT)
        self.assertEqual(restored.radio_names.get(0), "Woonkamer")
        self.assertEqual(restored.zones, {})
        other_controller = heat.HeatDemandLedger("01:111111")
        other_controller.restore_daily(stored, AT)
        self.assertEqual(other_controller.radio_names, {})


if __name__ == "__main__":
    unittest.main()
