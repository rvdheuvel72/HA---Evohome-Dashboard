#!/usr/bin/env python3
"""Controlled ID18/25/117 R8810A RF-path diagnostic for RAMSES Gateway beta.37+.

Runs on a computer that can reach the user's ESP32 over their trusted LAN.
Only authenticated, explicitly authorized single Read-Data (3220) requests;
no writes, binding, retries or automatic heating changes. Collects the
existing RAM-only packet capture and saves the complete evidence locally.
"""
from __future__ import annotations

import argparse
import base64
import csv
import getpass
import io
import json
import os
from pathlib import Path
import re
import time
from datetime import datetime, timezone
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode, urlsplit
from urllib.request import HTTPRedirectHandler, ProxyHandler, Request, build_opener

IDS = (18, 25, 117)
CAPTURE = "/api/v1/capture"
READ = "/api/v1/opentherm/paired-read"
STATUS = "/api/v1/status"
TERMINAL = {"read_ack", "data_invalid", "unknown_data_id", "timeout", "tx_failed"}
RESPONSE_TIMEOUT = 12.0  # observably > firmware's 1.8-second RF window
READY_TIMEOUT = 60.0
RF_PATTERN = re.compile(
    r"\b(RQ|RP|I|W)\s+---\s+(\d{2}:\d{6}|--:------)\s+"
    r"(\d{2}:\d{6}|--:------)\s+\S+\s+3220\s+005\s+([0-9A-Fa-f]{10})(?:\s|$)"
)


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        raise RuntimeError("Gateway redirected the request; refusing to forward credentials")


class GatewayError(RuntimeError):
    pass


class Gateway:
    def __init__(self, base_url: str, username: str, password: str):
        url = urlsplit(base_url)
        if url.scheme != "http" or not url.hostname or url.username or url.password or url.path not in ("", "/"):
            raise ValueError("Use the HTTP gateway root URL, without credentials or a path")
        self.root = base_url.rstrip("/")
        self._authorization = "Basic " + base64.b64encode(
            (username + ":" + password).encode("utf-8")
        ).decode("ascii")
        # No proxy or redirects: never forward HTTP Basic credentials elsewhere.
        self._opener = build_opener(ProxyHandler({}), NoRedirect())

    def request(self, method: str, path: str, fields: dict | None = None,
                config_header: str | None = None) -> str:
        if not path.startswith("/api/v1/") or ".." in path:
            raise ValueError("Unexpected gateway endpoint")
        headers = {"Authorization": self._authorization, "Cache-Control": "no-store"}
        body = None
        if fields is not None:
            body = urlencode(fields).encode("ascii")
            headers["Content-Type"] = "application/x-www-form-urlencoded"
        if config_header:
            headers["X-Gateway-Config"] = config_header
        req = Request(self.root + path, data=body, method=method, headers=headers)
        try:
            with self._opener.open(req, timeout=8) as answer:
                data = answer.read(180_000)
                return data.decode("utf-8", errors="replace")
        except HTTPError as e:
            try:
                payload = json.loads(e.read(2048).decode("utf-8"))
                reason = payload.get("error", "HTTP error")
            except (ValueError, UnicodeDecodeError):
                reason = "HTTP error"
            raise GatewayError(f"Gateway HTTP {e.code}: {reason}") from None
        except (URLError, TimeoutError) as e:
            raise GatewayError(f"Cannot reach the gateway: {type(e).__name__}") from None

    def get_json(self, path: str) -> dict:
        data = json.loads(self.request("GET", path))
        if not isinstance(data, dict):
            raise GatewayError("Unexpected JSON from gateway")
        return data

    def post(self, path: str, fields: dict, config_header: str) -> dict:
        data = json.loads(self.request("POST", path, fields, config_header))
        if not isinstance(data, dict):
            raise GatewayError("Unexpected POST response from gateway")
        return data

    def capture_action(self, action: str) -> dict:
        if action not in ("start", "stop"):
            raise ValueError("Unsupported capture operation")
        return self.post(CAPTURE + "/action", {"action": action}, "packet-capture")


def wait_ready(client, monotonic=time.monotonic, sleep=time.sleep, limit=READY_TIMEOUT):
    deadline = monotonic() + limit
    while True:
        status = client.get_json(READ)
        if status.get("ready") is True:
            return status
        if monotonic() >= deadline:
            raise GatewayError("Read preflight not ready: " + str(status.get("block_reason", "unknown")))
        sleep(min(0.7, max(0.05, deadline - monotonic())))


def one_read(client, data_id: int, monotonic=time.monotonic, sleep=time.sleep) -> dict:
    if data_id not in IDS:
        raise ValueError("Only diagnostic IDs 18, 25 and 117 are allowed")
    ready = wait_ready(client, monotonic, sleep)
    start = client.get_json(STATUS).get("uptime_ms")
    request = client.post(READ, {
        "data_id": str(data_id),
        "confirm": "one-read-own-r8810a-as-observed-controller",
    }, "paired-bridge-read")
    if request.get("queued") is not True or request.get("read_only") is not True:
        raise GatewayError("Gateway did not confirm a read-only queued request")
    deadline = monotonic() + RESPONSE_TIMEOUT
    while True:
        result = client.get_json(READ)
        if int(result.get("data_id", -1)) == data_id and result.get("state") in TERMINAL:
            break
        if monotonic() >= deadline:
            result = {"state": "unresolved_local_status", "data_id": data_id}
            break
        sleep(0.35)
    end = client.get_json(STATUS).get("uptime_ms")
    return {
        "id": data_id, "label": {18:"CH water pressure",25:"CH flow temperature",117:"CH pump starts"}[data_id],
        "preflight_controller": ready.get("controller_id"),
        "preflight_bridge": ready.get("bridge_id"),
        "start_uptime_ms_approx": start,
        "end_uptime_ms_approx": end,
        "state": result.get("state"), "raw": result.get("raw"),
        "value": result.get("value") if result.get("has_value") else None,
        "tx_phase": result.get("tx_phase"), "tx_local_complete": result.get("tx_local_complete"),
        "tx_bytes": result.get("tx_bytes"), "tx_expected_bytes": result.get("tx_expected_bytes"),
        "evidence_warning": "A matched reply at the shared controller address cannot prove it was caused by our request",
    }


def extract_3220(tsv: str) -> list[dict]:
    rows = csv.DictReader(io.StringIO("\n".join(
        line for line in tsv.splitlines() if line and not line.startswith("#")
    )), delimiter="\t")
    result = []
    for row in rows:
        if row.get("type") != "packet":
            continue
        m = RF_PATTERN.search(row.get("message", ""))
        if not m:
            continue
        verb, sender, target, hexpayload = m.groups()
        if not hexpayload.startswith("00"):
            continue
        # The outer RAMSES frame can be valid even when the nested
        # 32-bit OpenTherm word is not. Mirror OpenThermReadOnly.h:
        # a zero wrapper byte, reserved bits clear and even parity.
        ot_word = int(hexpayload[2:10], 16)
        ot_valid = ((ot_word.bit_count() & 1) == 0 and
                    (int(hexpayload[2:4], 16) & 0x0F) == 0)
        ot_type = (int(hexpayload[2:4], 16) >> 4) & 7
        data_id = int(hexpayload[4:6], 16)
        if data_id not in IDS:
            continue
        # Never classify a parity-invalid RF payload as a real negative
        # OpenTherm acknowledgement; keep it as invalid evidence.
        reply_type = {4:"read_ack",6:"data_invalid",7:"unknown_data_id"}
        ot_reply = reply_type.get(ot_type) if ot_valid else None
        result.append({
            "uptime_ms": int(row.get("uptime_ms") or 0),
            "utc_epoch_ms": row.get("utc_epoch_ms"),
            "verb": verb, "source": sender, "destination": target,
            "id": data_id, "ot_type": ot_type,
            "ot_valid": ot_valid, "ot_reply": ot_reply,
            "invalid_reason": None if ot_valid else "invalid_opentherm_parity_or_reserved_bits",
            "raw": hexpayload[6:10], "rssi_magnitude": row.get("rssi_magnitude"),
            "hgi": row["message"],
        })
    return result


def annotate_windows(reads: list[dict], decoded: list[dict]) -> None:
    """Correlate RX candidates by device uptime; never infer causality.

    Browser/HTTP query timing differs from radio task timing. A qualifying RP
    can also belong to an overlapping physical Evohome RQ to the same 01 ID.
    """
    for read in reads:
        begin, end = read.get("start_uptime_ms_approx"), read.get("end_uptime_ms_approx")
        read["matching_rx_candidates"] = []
        read["natural_rq_candidates"] = []
        if not isinstance(begin, int) or not isinstance(end, int) or end < begin:
            continue
        # Match *exact* RF addresses learned in the paired-read preflight.
        # Another Evohome installation may be in radio range.
        controller = read.get("preflight_controller")
        bridge = read.get("preflight_bridge")
        try:
            controller_addr = f"01:{int(controller):06d}"
            bridge_addr = f"10:{int(bridge):06d}"
            if not (1 <= int(controller) < 262143 and
                    1 <= int(bridge) < 262143):
                continue
        except (TypeError, ValueError):
            continue
        for entry in decoded:
            if entry["id"] != read["id"] or not (begin - 1500 <= entry["uptime_ms"] <= end + 750):
                continue
            if not entry.get("ot_valid", False):
                continue
            if (entry["verb"] == "RP" and entry["source"] == bridge_addr
                    and entry["destination"] == controller_addr
                    and entry["ot_reply"] in ("read_ack", "data_invalid", "unknown_data_id")):
                read["matching_rx_candidates"].append({
                    "uptime_ms": entry["uptime_ms"],
                    "approx_since_preflight_ms": entry["uptime_ms"] - begin,
                    "ot_reply": entry["ot_reply"], "hgi": entry["hgi"],
                })
            elif (entry["verb"] == "RQ" and entry["source"] == controller_addr
                    and entry["destination"] == bridge_addr
                    and entry["ot_type"] == 0):
                read["natural_rq_candidates"].append(entry["hgi"])
        read["causality"] = "unverified: shared physical Evohome RF identity"


def report_text(report: dict) -> str:
    out = ["# R8810A 3-ID RF Diagnostic", "",
           "The gateway sends RF 3220 OpenTherm Read-Data using the OBSERVED Evohome controller identity.",
           "Read-Ack proves a bridge-side RF response; it does not prove what passed on the wired OpenTherm interface.",
           "Timeout means inconclusive, NOT unsupported. No RF capture can observe the gateway's own RF TX directly.", "",
           "| Data-ID | Gateway response | Local TX | RF capture RP candidates |", "|---|---|---|---|"]
    for x in report["reads"]:
        seen = len(x.get("matching_rx_candidates", []))
        elapsed = x.get("end_uptime_ms_approx")
        start = x.get("start_uptime_ms_approx")
        duration = str(elapsed - start) + " ms measured window" if isinstance(elapsed, int) and isinstance(start, int) else "not known"
        out.append(f'| {x["id"]} {x["label"]} | {x["state"]} | {x.get("tx_phase") or "unknown"}; {duration} | {seen} |')
    out += ["", f'Capture full: **{report.get("capture_full", "unknown")}**',
            f'Capture errors: **{report.get("capture_error") or "none"}**', "",
            "## Decoded observed 3220 RF traffic (including natural controller requests)", ""]
    for z in report["decoded_3220"]:
        out.append(f'- +{z["uptime_ms"]} ms {z["verb"]} {z["source"]} → {z["destination"]} '
                   f'ID{z["id"]} {z["ot_reply"] or z.get("invalid_reason") or "RQ/other"}: `{z["hgi"]}`')
    if not report["decoded_3220"]:
        out.append("No qualifying 3220 frames captured; this may reflect RX misses or a full/empty capture.")
    if report.get("error"):
        out += ["", f'**Diagnostic interrupted:** {report["error"]}']
    out += ["", "## Interpretation", "",
            "- Only an explicit `unknown_data_id` says a queried identifier was rejected; it does not identify whether bridge or boiler rejected it.",
            "- A `timeout` cannot determine support and may reflect gateway timing, RF loss, bridge filtering, or unavailable boiler response.",
            "- To prove bridge transparency, record the wired OpenTherm traffic with an independent monitor as well."]
    return "\n".join(out) + "\n"


def run(client, output: Path, replace_capture: bool = False,
        monotonic=time.monotonic, sleep=time.sleep) -> dict:
    old = client.get_json(CAPTURE + "/status")
    if (old.get("active") or old.get("count", 0)) and not replace_capture:
        raise GatewayError(
            "Existing capture has data or is running. Export it first; use --replace-capture only after saving it."
        )
    output.mkdir(parents=True, exist_ok=False)
    report = {"created_utc": datetime.now(timezone.utc).isoformat(), "ids": list(IDS),
              "reads": [], "decoded_3220": [], "capture_full": None,
              "capture_error": None, "error": None}
    capture_started = False
    try:
        client.capture_action("start")
        capture_started = True
        for data_id in IDS:
            print(f"Testing OpenTherm ID {data_id}: explicitly authorized one-shot Read-Data")
            report["reads"].append(one_read(client, data_id, monotonic, sleep))
            x = report["reads"][-1]
            print(f"  {x['state']} · TX {x.get('tx_phase')}")
    except (GatewayError, RuntimeError, ValueError, KeyError, TypeError) as exc:
        report["error"] = str(exc)
    finally:
        capture_errors = []
        if capture_started:
            try:
                client.capture_action("stop")
            except Exception as exc:
                # Even malformed/truncated JSON from a successful stop must
                # NOT suppress capture download or writing the reports.
                capture_errors.append("stop: " + type(exc).__name__)
            try:
                final = client.get_json(CAPTURE + "/status")
                report["capture_full"] = final.get("full")
                report["capture_count"] = final.get("count")
            except Exception as exc:
                capture_errors.append("status: " + type(exc).__name__)
            try:
                capture = client.request("GET", CAPTURE + "/download")
                (output / "capture.tsv").write_text(capture, encoding="utf-8")
                report["decoded_3220"] = extract_3220(capture)
                annotate_windows(report["reads"], report["decoded_3220"])
            except Exception as exc:
                capture_errors.append("download: " + type(exc).__name__)
        report["capture_error"] = "; ".join(capture_errors) or None
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        (output / "report.md").write_text(report_text(report), encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gateway", default="http://evohome-gateway", help="LAN URL or local IP of ESP32")
    parser.add_argument("--user", required=True, help="Gateway web authentication user")
    parser.add_argument("--authorize-rf", action="store_true", help="Explicitly authorize three R8810A RF read requests as the existing Evohome controller identity (collision risk)")
    parser.add_argument("--replace-capture", action="store_true", help="Discard an existing RAM-only capture after exporting it separately")
    parser.add_argument("--output", type=Path, help="New output folder (must not exist)")
    args = parser.parse_args()
    if not args.authorize_rf:
        parser.error("--authorize-rf is mandatory: these requests spoof the observed existing Evohome controller RF identity")
    passwd = os.environ.get("EVOHOME_GATEWAY_PASSWORD") or getpass.getpass("ESP32 gateway password (not logged): ")
    if not passwd:
        parser.error("Gateway password required")
    output = args.output or Path("r8810a-diag-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
    try:
        report = run(Gateway(args.gateway, args.user, passwd), output, args.replace_capture)
    except (GatewayError, ValueError, OSError) as exc:
        print(f"Stopped without transmission: {exc}")
        return 2
    print("Report saved:", output.resolve())
    if report["error"] or report["capture_error"]:
        print("Diagnostic incomplete:", report["error"] or report["capture_error"])
        return 1
    print("Captured", len(report["decoded_3220"]), "relevant 3220 frames. No boiler-side forwarding conclusion is implied.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())