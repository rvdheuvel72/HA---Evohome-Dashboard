import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from release_local_time import annotate, release_header


class LocalReleaseTimeTests(unittest.TestCase):
    def test_summer_ceST(self):
        self.assertEqual(
            release_header("2026-10-14T07:24:00Z"),
            "**Release date (CET/CEST, Europe/Amsterdam): "
            "2026-10-14 09:24:00 CEST (UTC+02:00)**"
        )

    def test_winter_cet(self):
        self.assertEqual(
            release_header("2026-12-14T08:24:00Z"),
            "**Release date (CET/CEST, Europe/Amsterdam): "
            "2026-12-14 09:24:00 CET (UTC+01:00)**"
        )

    def test_dst_switch(self):
        self.assertIn("01:59:59 CET", release_header("2026-03-29T00:59:59Z"))
        self.assertIn("03:00:00 CEST", release_header("2026-03-29T01:00:00Z"))

    def test_idempotent_and_preserves_release_notes(self):
        initial = "# Improvements\n\n- No firmware changes.\n"
        first = annotate(initial, "2026-10-14T07:24:00Z")
        self.assertEqual(annotate(first, "2026-10-14T07:24:00Z"), first)
        self.assertEqual(first.count("Release date (CET/CEST"), 1)
        self.assertTrue(first.endswith(initial))

    def test_corrects_existing_localized_header(self):
        original = annotate("Notes", "2026-10-14T07:24:00Z")
        corrected = annotate(original, "2026-10-14T08:24:00Z")
        self.assertIn("10:24:00 CEST", corrected)
        self.assertNotIn("09:24:00 CEST", corrected)
        self.assertEqual(corrected.count("Release date (CET/CEST"), 1)

    def test_rejects_timestamp_without_timezone(self):
        with self.assertRaises(ValueError):
            release_header("2026-10-14T07:24:00")


if __name__ == "__main__":
    unittest.main()
