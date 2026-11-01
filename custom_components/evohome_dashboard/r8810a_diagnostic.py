"""Admin-only R8810A RF path diagnostics via existing ESP32 HTTP endpoints.

No firmware update, boiler write, binding, retry or RF auto-polling.
The service runs on Home Assistant's backend: browsers never connect to the
ESP32 or receive the gateway's HTTP credentials.
"""
from __future__ import annotations

import asyncio
import csv
from datetime import datetime, timezone
import io
import ipaddress
import json
import re

import aiohttp
from homeassistant.helpers.aiohttp_client import async_get_clientsession

IDS = (18, 25, 117)
READ = "/api/v1/opentherm/paired-read"
CAPTURE = "/api/v1/capture"
STATUS = "/api/v1/status"
TERMINAL = {"read_ack", "data_invalid", "unknown_data_id", "timeout", "tx_failed"}
CONFIRM = "authorize-three-rf-reads-observed-evohome-controller-identity"
MAX_CAPTURE_BYTES = 128 * 1024
RF_PATTERN = re.compile(
    r"\b(RQ|RP|I|W)\s+---\s+(\d{2}:\d{6}|--:------)\s+"
    r"(\d{2}:\d{6}|--:------)\s+\S+\s+3220\s+005\s+([0-9A-Fa-f]{10})(?:\s|$)"
)


class DiagnosticError(ValueError):
    """A safe-to-display error without credentials or unrestricted URLs."""


def valid_gateway_host(value: str) -> str:
    """Restrict the backend HTTP client to a local gateway, not arbitrary URLs."""
    if not isinstance(value, str):
        raise DiagnosticError("Gateway address must be a hostname or private IPv4 address")
    host = value.strip().lower()
    if host in ("evohome-gateway", "evohome-gateway.local"):
        return host
    try:
        addr = ipaddress.IPv4Address(host)
    except ipaddress.AddressValueError as err:
        raise DiagnosticError("Use evohome-gateway or a private LAN IPv4 address") from err
    if not (addr.is_private and not addr.is_loopback and not addr.is_link_local
            and not addr.is_multicast and not addr.is_reserved and not addr.is_unspecified):
        raise DiagnosticError("Gateway must use a private LAN IPv4 address")
    return str(addr)


def address(prefix: str, numeric: object) -> str | None:
    """The firmware reports decimal RAMSES IDs without device-type prefixes."""
    try:
        value = int(numeric)
    except (ValueError, TypeError):
        return None
    if not 0 < value <= 262143:
        return None
    return f"{prefix}:{value:06d}"


def decode_capture(text: str) -> list[dict]:
    """Return only nested OpenTherm-valid frames for tracked test IDs.

    Invalid embedded OT parity is retained as invalid evidence but never
    presented as a bridge acknowledgement.
    """
    rows = csv.DictReader(io.StringIO("\n".join(
        line for line in text.splitlines() if line and not line.startswith("#")
    )), delimiter="\t")
    found = []
    for row in rows:
        if row.get("type") != "packet":
            continue
        match = RF_PATTERN.search(row.get("message", ""))
        if not match:
            continue
        verb, source, destination, payload = match.groups()
        if not payload.startswith("00"):
            continue
        word = int(payload[2:], 16)
        data_id = (word >> 16) & 0xFF
        if data_id not in IDS:
            continue
        valid = word.bit_count() % 2 == 0 and ((word >> 24) & 0x0F) == 0
        kind = (word >> 28) & 7
        reply = {4: "read_ack", 6: "data_invalid", 7: "unknown_data_id"}.get(kind) if valid else None
        try:
            uptime = int(row.get("uptime_ms") or 0)
        except ValueError:
            continue
        found.append({
            "id": data_id, "verb": verb, "source": source, "destination": destination,
            "uptime_ms": uptime, "ot_valid": valid, "ot_reply": reply,
            "message": row["message"][:200],
        })
    return found


def annotate(reads: list[dict], packets: list[dict]) -> None:
    for read in reads:
        begin, end = read.get("start_uptime_ms"), read.get("end_uptime_ms")
        bridge = address("10", read.get("bridge_id"))
        controller = address("01", read.get("controller_id"))
        read["rf_reply_candidates"] = []
        read["natural_request_candidates"] = []
        if not bridge or not controller or not isinstance(begin, int) or not isinstance(end, int):
            continue
        for frame in packets:
            if (frame["id"] != read["id"] or not frame["ot_valid"]
                    or not begin - 1500 <= frame["uptime_ms"] <= end + 750):
                continue
            if (frame["verb"] == "RP" and frame["source"] == bridge
                    and frame["destination"] == controller and frame["ot_reply"]):
                read["rf_reply_candidates"].append(frame)
            elif (frame["verb"] == "RQ" and frame["source"] == controller
                  and frame["destination"] == bridge):
                read["natural_request_candidates"].append(frame)


class GatewayHTTP:
    """Aiohttp LAN transport; no proxy redirects, credential forwarding or URLs from WS."""

    def __init__(self, hass, host: str, username: str, password: str):
        self.host = valid_gateway_host(host)
        self.session = async_get_clientsession(hass)
        self.auth = aiohttp.BasicAuth(username, password) if username and password else None

    async def _request(self, method: str, path: str, *, fields=None, config=None, json_result=True):
        if path not in (READ, STATUS, CAPTURE + "/status", CAPTURE + "/download",
                        CAPTURE + "/action"):
            raise DiagnosticError("Unexpected diagnostic endpoint")
        headers = {"Cache-Control": "no-store"}
        if config:
            headers["X-Gateway-Config"] = config
        try:
            async with self.session.request(
                method, f"http://{self.host}{path}",
                auth=self.auth, data=fields, headers=headers, allow_redirects=False,
                timeout=aiohttp.ClientTimeout(total=10),
            ) as response:
                if response.status in (301, 302, 303, 307, 308):
                    raise DiagnosticError("Gateway redirect refused")
                if response.status not in (200, 202):
                    if response.status in (401, 403):
                        raise DiagnosticError("Gateway authentication or RF authorization rejected")
                    if response.status == 409:
                        raise DiagnosticError("Gateway read not ready or in RF cooldown")
                    raise DiagnosticError(f"Gateway HTTP {response.status}")
                data = await response.content.read(MAX_CAPTURE_BYTES + 1)
                if len(data) > MAX_CAPTURE_BYTES:
                    raise DiagnosticError("Gateway response exceeds capture size limit")
                if not json_result:
                    return data.decode("utf-8", errors="replace")
                try:
                    result = json.loads(data)
                except (ValueError, UnicodeError) as err:
                    raise DiagnosticError("Gateway returned invalid JSON") from err
                if not isinstance(result, dict):
                    raise DiagnosticError("Gateway returned unexpected JSON")
                return result
        except (aiohttp.ClientError, asyncio.TimeoutError) as err:
            raise DiagnosticError(f"Gateway connection failed ({type(err).__name__})") from None

    async def get(self, path):
        return await self._request("GET", path)

    async def post(self, path, fields, config):
        return await self._request("POST", path, fields=fields, config=config)

    async def capture(self):
        return await self._request("GET", CAPTURE + "/download", json_result=False)


class R8810aDiagnostic:
    """One task per HA config entry, with bounded evidence kept in memory."""

    def __init__(self, api, *, sleep=asyncio.sleep):
        self.api = api
        self._sleep = sleep
        self._lock = asyncio.Lock()
        self._task: asyncio.Task | None = None
        self._selected_ids = IDS
        self.state = "idle"
        self.stage = ""
        self.error = None
        self.capture_error = None
        self.reads: list[dict] = []
        self.capture_text = ""
        self.packets: list[dict] = []
        self.capture_full = None
        self.started_at = None

    def snapshot(self):
        return {
            "configured": True,
            "running": self._task is not None and not self._task.done(),
            "state": self.state, "stage": self.stage, "error": self.error,
            "capture_error": self.capture_error, "capture_full": self.capture_full,
            "started_at": self.started_at, "reads": self.reads,
            "capture_available": bool(self.capture_text),
            "packet_count": len(self.packets), "selected_ids": list(self._selected_ids),
            "limits": "RF observations cannot establish wired OpenTherm forwarding",
        }

    async def start(self, *, confirm: str, data_id: int = 0,
                    replace_capture: bool = False):
        if confirm != CONFIRM:
            raise DiagnosticError("Explicit observed-controller RF authorization required")
        if data_id not in (0,) + IDS:
            raise DiagnosticError("Only IDs 18, 25 and 117 are allowed")
        async with self._lock:
            if self._task is not None and not self._task.done():
                raise DiagnosticError("A diagnostic is already running")
            capture = await self.api.get(CAPTURE + "/status")
            if (capture.get("active") or capture.get("count", 0)) and not replace_capture:
                raise DiagnosticError(
                    "Existing ESP32 packet capture present. Export it first, "
                    "then explicitly authorize replacing the capture."
                )
            self.state, self.stage, self.error = "running", "Starting RF capture", None
            self.capture_error = None
            self.capture_text = ""
            self.packets = []
            self.reads = []
            self.capture_full = None
            self._selected_ids = IDS if data_id == 0 else (data_id,)
            self.started_at = datetime.now(timezone.utc).isoformat()
            # Task owned by HA service; the browser may close or lose connection.
            self._task = asyncio.create_task(self._run())
        return self.snapshot()

    async def _wait_ready(self):
        deadline = asyncio.get_running_loop().time() + 65
        while True:
            item = await self.api.get(READ)
            if item.get("ready") is True:
                return item
            if asyncio.get_running_loop().time() >= deadline:
                raise DiagnosticError("Gateway read not ready or RF cooldown did not clear")
            await self._sleep(0.8)

    async def _read(self, data_id: int):
        self.stage = f"Waiting for gateway readiness: ID{data_id}"
        info = await self._wait_ready()
        start = (await self.api.get(STATUS)).get("uptime_ms")
        self.stage = f"Read-Data ID{data_id} RF request"
        queued = await self.api.post(READ, {
            "data_id": str(data_id),
            "confirm": "one-read-own-r8810a-as-observed-controller",
        }, "paired-bridge-read")
        if queued.get("queued") is not True or queued.get("read_only") is not True:
            raise DiagnosticError("Gateway did not confirm a queued read-only request")
        self.stage = f"Awaiting R8810A RF response for ID{data_id}"
        deadline = asyncio.get_running_loop().time() + 13
        while True:
            result = await self.api.get(READ)
            if result.get("data_id") == data_id and result.get("state") in TERMINAL:
                break
            if asyncio.get_running_loop().time() >= deadline:
                result = {"state": "unresolved_local_status"}
                break
            await self._sleep(0.4)
        end = (await self.api.get(STATUS)).get("uptime_ms")
        row = {
            "id": data_id, "label": {
                18: "Water pressure", 25: "Boiler flow temperature",
                117: "CH circulation pump starts",
            }[data_id],
            "state": result.get("state", "unresolved_local_status"),
            "value": result.get("value") if result.get("has_value") else None,
            "raw": result.get("raw"), "tx_phase": result.get("tx_phase"),
            "tx_bytes": result.get("tx_bytes"), "tx_expected_bytes": result.get("tx_expected_bytes"),
            "start_uptime_ms": start, "end_uptime_ms": end,
            "controller_id": info.get("controller_id"), "bridge_id": info.get("bridge_id"),
            "caveat": "Matching RF reply may belong to the physical Evohome controller",
        }
        self.reads.append(row)

    async def _run(self):
        capture_started = False
        try:
            await self.api.post(CAPTURE + "/action", {"action": "start"}, "packet-capture")
            capture_started = True
            for data_id in self._selected_ids:
                await self._read(data_id)
            self.state = "complete"
        except asyncio.CancelledError:
            self.error = "Home Assistant diagnostic stopped or integration unloaded"
            self.state = "interrupted"
        except Exception as err:
            self.error = str(err) if isinstance(err, DiagnosticError) else type(err).__name__
            self.state = "interrupted"
        finally:
            errors = []
            if capture_started:
                self.stage = "Stopping and collecting RF capture"
                try:
                    await self.api.post(CAPTURE + "/action", {"action": "stop"}, "packet-capture")
                except Exception as err:
                    errors.append("stop: " + type(err).__name__)
                try:
                    end_status = await self.api.get(CAPTURE + "/status")
                    self.capture_full = end_status.get("full")
                except Exception as err:
                    errors.append("capture status: " + type(err).__name__)
                try:
                    self.capture_text = await self.api.capture()
                    self.packets = decode_capture(self.capture_text)
                    annotate(self.reads, self.packets)
                except Exception as err:
                    errors.append("capture download: " + type(err).__name__)
            self.capture_error = "; ".join(errors) or None
            self.stage = "Completed" if self.state == "complete" else "Incomplete"

    async def stop(self):
        """Do not leave an RF/capture task running when HA unloads an entry."""
        if self._task is not None and not self._task.done():
            self._task.cancel()
            try:
                await asyncio.wait_for(self._task, timeout=15)
            except (asyncio.CancelledError, asyncio.TimeoutError):
                pass

    async def existing_capture(self):
        # Non-destructive export before starting a new RAM-only RF capture.
        status = await self.api.get(CAPTURE + "/status")
        return {"status": status, "capture_tsv": await self.api.capture()}

    def export(self):
        if self._task is not None and not self._task.done():
            raise DiagnosticError("Diagnostic is still running")
        return {
            "report": {
                "created_at": self.started_at, "state": self.state,
                "error": self.error, "capture_error": self.capture_error,
                "capture_full": self.capture_full, "reads": self.reads,
                "packets": self.packets,
                "interpretation": (
                    "Timeout is inconclusive, not an unsupported-ID declaration. "
                    "Observed RF packets do not prove wired OpenTherm forwarding."
                ),
            },
            "capture_tsv": self.capture_text,
        }
