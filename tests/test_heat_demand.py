"""Pure, hardware-free validation of passive room demand and RF safety boundaries."""
from __future__ import annotations

import importlib.util
import json
import sys
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

SOURCE = (Path(__file__).resolve().parents[1] / "custom_components" /
          "evohome_dashboard" / "heat_demand.py")
spec = importlib.util.spec_from_file_location("testable_heat_demand", SOURCE)
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)

HeatDemandLedger = module.HeatDemandLedger
parse_frame = module.parse_frame
valid_zone_names = module.valid_zone_names

CTL = "01:245914"
BRIDGE = "10:048994"
TRV_A = "04:056057"
TRV_B = "04:056058"
BASE = datetime(2026, 10, 31, 8, tzinfo=timezone.utc)


def frame(source=TRV_A, controller=CTL, index="03", raw="26"):
    return parse_frame(
        f"045  I --- {source} --:------ {controller} 3150 002 {index}{raw}"
    )


def mqtt_payload(text, timestamp=BASE):
    return json.dumps({"msg": text, "ts": timestamp.isoformat()})


class HeatDemandTest(unittest.TestCase):
    def setUp(self):
        self.ledger = HeatDemandLedger(CTL, zone_names={3: "Living room"})

    def test_parse_realistic_ramses_line(self):
        packet = frame()
        self.assertEqual(packet.source, TRV_A)
        self.assertEqual(packet.payload, bytes.fromhex("0326"))
        self.assertIsNone(parse_frame("I 04:056057 3150 0326"))
        self.assertIsNone(parse_frame(
            "045  I --- 04:056057 --:------ 01:245914 3150 003 0326"))
        self.assertIsNone(parse_frame(
            "045  I --- 04:056057 --:------ 01:245914 3150 002 03FF"))

    def test_demand_requires_correct_controller_and_source(self):
        self.assertFalse(self.ledger.observe(frame(controller="01:111111"), BASE))
        self.assertFalse(self.ledger.observe(frame(source="04:000001", index="FC"), BASE))
        self.assertFalse(self.ledger.observe(frame(source="12:160001"), BASE))
        self.assertFalse(self.ledger.observe(frame(raw="FF"), BASE))
        self.assertEqual(self.ledger.zones, {})
        self.assertTrue(self.ledger.observe(frame(), BASE))
        self.assertEqual(self.ledger.snapshot(BASE)["zones"][0]["demand_pct"], 19)

    def test_multiple_actuators_and_weighted_time(self):
        self.ledger.observe(frame(), BASE)
        self.ledger.observe(frame(source=TRV_B, raw="A0"), BASE + timedelta(minutes=5))
        zone = self.ledger.snapshot(BASE + timedelta(minutes=10))["zones"][0]
        self.assertEqual(zone["name"], "Living room")
        self.assertEqual(zone["demand_pct"], 80)
        self.assertEqual(zone["today_minutes"], 10)
        self.assertEqual(zone["today_weighted_minutes"], 5.0)
        self.assertEqual(len(zone["devices"]), 2)

    def test_stale_does_not_mean_off_and_capped_duration(self):
        self.ledger.observe(frame(raw="C8"), BASE)
        zone = self.ledger.snapshot(BASE + timedelta(minutes=40))["zones"][0]
        self.assertIsNone(zone["demand_pct"])
        self.assertEqual(zone["state"], "unknown_stale")
        self.assertEqual(zone["today_minutes"], 25)
        self.assertEqual(zone["today_weighted_minutes"], 25)

    def test_explicit_zero_ends_demand(self):
        self.ledger.observe(frame(raw="C8"), BASE)
        self.ledger.observe(frame(raw="00"), BASE + timedelta(minutes=9))
        zone = self.ledger.snapshot(BASE + timedelta(minutes=20))["zones"][0]
        self.assertEqual(zone["state"], "idle")
        self.assertEqual(zone["today_minutes"], 9)

    def test_rejected_old_timestamp_and_mqtt_input(self):
        line = f"045  I --- {TRV_A} --:------ {CTL} 3150 002 0326"
        accepted, discovered = self.ledger.observe_mqtt(
            mqtt_payload(line), BASE + timedelta(seconds=2)
        )
        self.assertTrue(accepted)
        self.assertEqual(discovered, 3)
        accepted, discovered = self.ledger.observe_mqtt(
            mqtt_payload(line), BASE + timedelta(minutes=8)
        )
        self.assertFalse(accepted)
        self.assertIsNone(discovered)
        self.assertFalse(self.ledger.observe_mqtt("{" + "x" * 3000, BASE)[0])

    def test_controller_0008_not_conflated_with_trv(self):
        ctl = parse_frame(
            f"045  I --- {CTL} {BRIDGE} --:------ 0008 002 00BE"
        )
        self.assertTrue(self.ledger.observe(ctl, BASE))
        self.assertEqual(self.ledger.snapshot(BASE)["controller_0008_pct"], 95)
        trv = parse_frame(
            f"045  I --- 12:160001 --:------ {CTL} 0008 002 00BE"
        )
        self.assertFalse(self.ledger.observe(trv, BASE))
        self.assertEqual(self.ledger.snapshot(BASE)["controller_0008_pct"], 95)
        self.assertIsNone(self.ledger.snapshot(BASE + timedelta(minutes=30))["controller_0008_pct"])

    def test_bridge_exchange_only_not_burner_activity(self):
        packet = parse_frame(
            f"096 RP --- {BRIDGE} {CTL} --:------ 3EF0 001 00"
        )
        self.assertTrue(self.ledger.observe(packet, BASE))
        self.assertEqual(self.ledger.snapshot(BASE)["bridge_frames"], 1)

    def test_restart_restores_only_daily_totals_not_rf_state(self):
        self.ledger.observe(frame(raw="C8"), BASE)
        saved = self.ledger.daily_storage(BASE + timedelta(minutes=10))
        replacement = HeatDemandLedger(CTL)
        replacement.restore_daily(saved, BASE + timedelta(minutes=12))
        zone = replacement.snapshot(BASE + timedelta(minutes=14))["zones"][0]
        self.assertEqual(zone["today_minutes"], 10)
        self.assertIsNone(zone["demand_pct"])
        replacement.restore_daily(saved, BASE + timedelta(days=1))
        self.assertEqual(replacement.snapshot(BASE + timedelta(days=1))["zones"][0]["today_minutes"], 0)

    def test_midnight_bucket_uses_local_timezone(self):
        # The local midnight falls at 23:00 UTC before the autumn DST change.
        start = datetime(2026, 10, 24, 22, 50, tzinfo=timezone.utc)
        self.ledger.observe(frame(raw="C8"), start)
        zone = self.ledger.snapshot(start + timedelta(minutes=20))["zones"][0]
        self.assertEqual(zone["today_minutes"], 10)

    def test_room_mapping_validation(self):
        self.assertEqual(valid_zone_names('{"03":"Living room","0A":"Bedroom"}'),
                         {3: "Living room", 10: "Bedroom"})
        for bad in ('{"0C":"Outside range"}', '["not", "a", "map"]',
                    '{"03":""}', '{"03":2}', '{"03":""'):
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    valid_zone_names(bad)


if __name__ == "__main__":
    unittest.main()
