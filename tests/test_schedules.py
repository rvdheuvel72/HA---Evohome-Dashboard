"""Pure schema validation: no Home Assistant or cloud account required."""
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1] / "custom_components/evohome_dashboard/schedules.py"
spec = importlib.util.spec_from_file_location("evohome_schedule_pure", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

DAYS = module.DAYS
validate = module.validate
digest = module.digest
normalize = module.normalize


def good():
    return [{"day_of_week": name, "switchpoints": [
        {"time_of_day": "06:30:00", "heat_setpoint": 20.5},
        {"time_of_day": "22:00:00", "heat_setpoint": 16.0},
    ]} for name in DAYS]


class ScheduleTests(unittest.TestCase):
    def test_valid_week(self):
        self.assertEqual(validate(good()), good())

    def test_hash_is_stable(self):
        self.assertEqual(digest(good()), digest(good()))

    def test_detect_optimistic_concurrency_conflict(self):
        old = digest(good())
        newer = good()
        newer[0]["switchpoints"][0]["heat_setpoint"] = 21.0
        self.assertNotEqual(digest(newer), old)

    def test_reject_missing_days(self):
        with self.assertRaises(ValueError):
            validate(good()[:6])

    def test_reject_duplicate_day(self):
        bad = good()
        bad[-1]["day_of_week"] = "monday"
        with self.assertRaises(ValueError):
            validate(bad)

    def test_reject_unordered_switchpoints(self):
        bad = good()
        bad[0]["switchpoints"].reverse()
        with self.assertRaises(ValueError):
            validate(bad)

    def test_reject_outside_temperature_range(self):
        bad = good()
        bad[0]["switchpoints"][0]["heat_setpoint"] = 50
        with self.assertRaises(ValueError):
            validate(bad)

    def test_reject_invalid_temperature_resolution(self):
        bad = good()
        bad[0]["switchpoints"][0]["heat_setpoint"] = 20.3
        with self.assertRaises(ValueError):
            validate(bad)

    def test_reject_invalid_timing_resolution(self):
        bad = good()
        bad[0]["switchpoints"][0]["time_of_day"] = "06:35:00"
        with self.assertRaises(ValueError):
            validate(bad)

    def test_normalize_vendor_shape(self):
        src = [{"dayOfWeek": "Monday", "switchpoints": [
            {"timeOfDay": "06:30:00", "heatSetpoint": 21.0},
        ]}]
        self.assertEqual(normalize(src)[0]["day_of_week"], "monday")
        self.assertEqual(normalize(src)[0]["switchpoints"][0]["heat_setpoint"], 21.0)


if __name__ == "__main__":
    unittest.main()
