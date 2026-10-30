#!/usr/bin/env python3
"""Optional live ESP32 diagnostics regression probe (no hardware in CI).

Example:
  GW_DIAG_USER=admin GW_DIAG_PASSWORD=... python3 scripts/check_gateway_health.py \
    --url http://evohome-gateway.local --samples 12 --interval 5 \
    --min-heap 25000 --min-stack 700 --max-http-ms 1500

Thresholds must come from an actual baseline on the target hardware. The
lifetime http_max_ms counter can include earlier network outages.
"""
import argparse
import base64
import json
import os
import statistics
import sys
import time
from urllib.request import Request, urlopen


FIELDS = (
    "free_heap", "min_free_heap", "largest_free_heap_block",
    "radio_task_stack_min_free_bytes", "http_last_ms", "http_max_ms",
    "http_slow_calls", "web_asset_last_ms", "web_asset_max_ms",
    "uptime_ms",
)


def read_status(origin, timeout):
    headers = {"Accept": "application/json"}
    user, password = os.getenv("GW_DIAG_USER"), os.getenv("GW_DIAG_PASSWORD")
    if user is not None and password is not None:
        basic = base64.b64encode((user + ":" + password).encode()).decode()
        headers["Authorization"] = "Basic " + basic
    request = Request(origin.rstrip("/") + "/api/v1/status", headers=headers)
    started = time.monotonic()
    with urlopen(request, timeout=timeout) as response:
        if response.status != 200:
            raise RuntimeError("Gateway HTTP status " + str(response.status))
        data = json.load(response)
    return data, round((time.monotonic() - started) * 1000, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True, help="HTTP gateway origin")
    parser.add_argument("--samples", type=int, default=12)
    parser.add_argument("--interval", type=float, default=5)
    parser.add_argument("--timeout", type=float, default=5)
    parser.add_argument("--min-heap", type=int)
    parser.add_argument("--min-stack", type=int)
    parser.add_argument("--max-http-ms", type=int)
    args = parser.parse_args()
    if not (1 <= args.samples <= 300) or args.interval < 0 or args.timeout <= 0:
        parser.error("samples 1..300, interval >=0 and timeout >0 required")
    data_points, latencies = [], []
    for i in range(args.samples):
        data, elapsed_ms = read_status(args.url, args.timeout)
        for key in FIELDS:
            if not isinstance(data.get(key), int) or data[key] < 0:
                raise ValueError("Missing or invalid telemetry: " + key)
        data_points.append(data)
        latencies.append(elapsed_ms)
        if i + 1 < args.samples:
            time.sleep(args.interval)

    result = {
        "samples": len(data_points),
        "min_observed_free_heap_bytes": min(x["free_heap"] for x in data_points),
        "min_lifetime_free_heap_bytes": min(x["min_free_heap"] for x in data_points),
        "min_largest_free_block_bytes": min(x["largest_free_heap_block"] for x in data_points),
        "min_observed_rx_stack_free_bytes": min(x["radio_task_stack_min_free_bytes"] for x in data_points),
        "max_gateway_http_handler_ms": max(x["http_max_ms"] for x in data_points),
        "max_client_response_ms": max(latencies),
        "mean_client_response_ms": round(statistics.mean(latencies), 1),
        "latest_uptime_ms": data_points[-1]["uptime_ms"],
    }
    print(json.dumps(result, indent=2))
    failures = []
    if args.min_heap is not None:
        if result["min_lifetime_free_heap_bytes"] < args.min_heap:
            failures.append("min_free_heap below baseline threshold")
        if result["min_largest_free_block_bytes"] < args.min_heap:
            failures.append("largest free block below baseline threshold")
    if args.min_stack is not None and result["min_observed_rx_stack_free_bytes"] < args.min_stack:
        failures.append("RX stack watermark below baseline or not sampled yet")
    if args.max_http_ms is not None and result["max_gateway_http_handler_ms"] > args.max_http_ms:
        failures.append("lifetime HTTP handler maximum above baseline threshold")
    if failures:
        for failure in failures:
            print("FAIL: " + failure, file=sys.stderr)
        return 1
    if not any(x is not None for x in (args.min_heap, args.min_stack, args.max_http_ms)):
        print("BASELINE ONLY: no thresholds provided; no performance pass claim")
    else:
        print("All configured live regression thresholds passed")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print("Gateway probe failed: " + type(exc).__name__, file=sys.stderr)
        sys.exit(2)
