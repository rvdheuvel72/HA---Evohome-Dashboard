"""Regression tests for Codex P2: time-only override changes must be verified."""
import importlib.util
from datetime import datetime, timedelta, timezone
from pathlib import Path
import unittest

PATH = Path(__file__).resolve().parents[1] / "custom_components/evohome_dashboard/verification.py"
spec = importlib.util.spec_from_file_location("evohome_override_verification", PATH)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

expiry = mod.matches_override_expiry
t0 = datetime(2026, 10, 11, 17, 0, 0, tzinfo=timezone.utc)


class TestOverrideExpiry(unittest.TestCase):
    def test_reported_same_mode_but_old_expiry_is_not_verification(self):
        self.assertFalse(expiry(t0 + timedelta(hours=1), t0 + timedelta(hours=2)))

    def test_exact_new_expiry_succeeds(self):
        self.assertTrue(expiry(t0 + timedelta(hours=2), t0 + timedelta(hours=2)))

    def test_subminute_truncation_is_accepted(self):
        self.assertTrue(expiry(t0, t0 + timedelta(seconds=35)))

    def test_full_subminute_truncation_window(self):
        self.assertTrue(expiry(t0, t0 + timedelta(seconds=59, milliseconds=999)))
        self.assertFalse(expiry(t0, t0 + timedelta(seconds=60)))

    def test_different_minute_is_not_verification(self):
        self.assertFalse(expiry(t0, t0 + timedelta(minutes=2)))

    def test_timezone_equivalence(self):
        local = timezone(timedelta(hours=2))
        self.assertTrue(expiry(t0.astimezone(local), t0))

    def test_missing_actual_expiry_fails_temporary(self):
        self.assertFalse(expiry(None, t0))

    def test_permanent_requires_missing_actual_expiry(self):
        self.assertTrue(expiry(None, None))
        self.assertFalse(expiry(t0, None))

    def test_naive_datetime_is_not_verification(self):
        self.assertFalse(expiry(t0.replace(tzinfo=None), t0))
        self.assertFalse(expiry(t0, t0.replace(tzinfo=None)))


if __name__ == "__main__":
    unittest.main()
