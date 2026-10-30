"""Pure tests for optional live gateway performance-budget evaluation."""
import contextlib
import io
import sys
import unittest
from unittest.mock import patch

from scripts import check_gateway_health as probe


def sample(**overrides):
    data = {
        "free_heap": 80000,
        "min_free_heap": 72000,
        "largest_free_heap_block": 50000,
        "radio_task_stack_min_free_bytes": 1550,
        "http_last_ms": 10,
        "http_max_ms": 140,
        "http_slow_calls": 0,
        "web_asset_last_ms": 30,
        "web_asset_max_ms": 110,
        "uptime_ms": 300000,
    }
    data.update(overrides)
    return data


class GatewayHealthTest(unittest.TestCase):
    def run_probe(self, payload, *arguments):
        with patch.object(sys, "argv", ["check_gateway_health", "--url", "http://127.0.0.1",
                                         "--samples", "1", *arguments]), \
             patch.object(probe, "read_status", return_value=(payload, 22.0)), \
             contextlib.redirect_stdout(io.StringIO()) as stdout, \
             contextlib.redirect_stderr(io.StringIO()) as stderr:
            result = probe.main()
        return result, stdout.getvalue(), stderr.getvalue()

    def test_baseline_only_does_not_claim_threshold_pass(self):
        code, out, _ = self.run_probe(sample())
        self.assertEqual(code, 0)
        self.assertIn("BASELINE ONLY", out)

    def test_healthy_measured_thresholds_pass(self):
        code, out, _ = self.run_probe(sample(), "--min-heap", "40000",
                                      "--min-stack", "1000",
                                      "--max-http-ms", "200")
        self.assertEqual(code, 0)
        self.assertIn("thresholds passed", out)

    def test_heap_fragmentation_triggers_failure(self):
        code, _, err = self.run_probe(sample(largest_free_heap_block=12000),
                                      "--min-heap", "25000")
        self.assertEqual(code, 1)
        self.assertIn("largest free block", err)

    def test_stack_not_sampled_is_not_false_success(self):
        code, _, err = self.run_probe(sample(radio_task_stack_min_free_bytes=0),
                                      "--min-stack", "800")
        self.assertEqual(code, 1)
        self.assertIn("RX stack watermark", err)

    def test_missing_telemetry_fails(self):
        payload = sample()
        del payload["http_max_ms"]
        with self.assertRaises(ValueError):
            self.run_probe(payload)


if __name__ == "__main__":
    unittest.main()
