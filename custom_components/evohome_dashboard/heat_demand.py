"""Passive RAMSES-II heat-demand observations and bounded, restart-safe statistics.

No RF transmit, valve writes, OpenTherm commands or gas attribution.
The RF codes are observed signals; controller 0008 is NOT proof of boiler firing.
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from datetime import date, datetime, time, timedelta, timezone
from zoneinfo import ZoneInfo

MAX_AGE = timedelta(minutes=25)  # Approximate RAMSES demand report expiry.
MAX_ZONES = 12
MAX_DEVICES_PER_ZONE = 8
MAX_RECENT = 32
_ID = re.compile(r"^(?:[0-9]{2}:[0-9]{6}|--:------)$")
_LINE = re.compile(
    r"^\s*\d{3}\s+(RQ|RP|I|W)\s+(?:---|\d{3})\s+"
    r"(\S+)\s+(\S+)\s+(\S+)\s+([0-9A-Fa-f]{4})\s+(\d{3})\s+([0-9A-Fa-f]*)\s*$"
)


@dataclass(frozen=True)
class Frame:
    verb: str
    source: str
    addresses: tuple[str, str, str]
    code: str
    payload: bytes


def parse_frame(text: str) -> Frame | None:
    """Parse only well-formed gateway/HGI lines; never infer from partial frames."""
    if not isinstance(text, str) or len(text) > 320:
        return None
    match = _LINE.fullmatch(text)
    if match is None:
        return None
    verb, source, second, third, code, length_text, raw = match.groups()
    addresses = (source, second, third)
    if any(not _ID.fullmatch(a) for a in addresses):
        return None
    length = int(length_text)
    if length > 64 or len(raw) != 2 * length:
        return None
    return Frame(verb, source, addresses, code.upper(), bytes.fromhex(raw))


def demand_pct(raw: int) -> float | None:
    """RAMSES demand units are half-percent (0xC8 == 100%)."""
    if not 0 <= raw <= 200:
        return None
    return raw / 2.0


def valid_controller_id(value: str) -> bool:
    return isinstance(value, str) and re.fullmatch(r"01:\d{6}", value) is not None


def valid_zone_names(text: str) -> dict[int, str]:
    if not text:
        return {}
    value = json.loads(text)
    if not isinstance(value, dict):
        raise ValueError("RF room mapping must be a JSON object")
    result = {}
    for key, name in value.items():
        if not isinstance(key, str) or not re.fullmatch(r"(?:0[0-9]|0[AB])", key.upper()):
            raise ValueError("Room keys must be hex zone indices 00..0B")
        if not isinstance(name, str) or not 1 <= len(name.strip()) <= 60:
            raise ValueError("Each room name must contain 1-60 characters")
        result[int(key, 16)] = name.strip()
    return result


@dataclass
class DeviceState:
    value: float
    seen: datetime


@dataclass
class ZoneState:
    devices: dict[str, DeviceState] = field(default_factory=dict)
    day: date | None = None
    on_seconds: float = 0.0
    weighted_seconds: float = 0.0
    tick: datetime | None = None
    samples: int = 0
    last_seen: datetime | None = None


class HeatDemandLedger:
    """Pure state machine. Monotonic RF timestamps, TTL and DST-safe day buckets."""

    def __init__(self, controller_id: str, *, zone_names=None, time_zone="Europe/Amsterdam"):
        if not valid_controller_id(controller_id):
            raise ValueError("Explicit RAMSES controller 01:xxxxxx ID required")
        self.controller_id = controller_id
        self.zone_names = dict(zone_names or {})
        self.tz = ZoneInfo(time_zone)
        self.zones: dict[int, ZoneState] = {}
        self.controller_demand: DeviceState | None = None
        self.bridge_last_seen: datetime | None = None
        self.bridge_frames = 0
        self.ignored = 0
        self.received = 0
        self.recent = []

    def _day(self, instant: datetime) -> date:
        return instant.astimezone(self.tz).date()

    def _current(self, z: ZoneState, when: datetime) -> float | None:
        values = [d.value for d in z.devices.values()
                  if d.seen <= when < d.seen + MAX_AGE]
        return max(values) if values else None

    def _advance(self, z: ZoneState, when: datetime) -> None:
        if z.tick is None:
            z.tick = when
            z.day = self._day(when)
            return
        if when <= z.tick:
            return
        cursor = z.tick
        # Divide intervals at RF expiry and local midnight; never count missing
        # frames as continuous heat demand beyond the freshness boundary.
        while cursor < when:
            active = [d for d in z.devices.values()
                      if d.seen <= cursor < d.seen + MAX_AGE]
            if not active:
                break
            local = cursor.astimezone(self.tz)
            next_day = datetime.combine(local.date() + timedelta(days=1),
                                        time.min, self.tz).astimezone(timezone.utc)
            until = min(when, next_day,
                        *(d.seen + MAX_AGE for d in active))
            if until <= cursor:
                break
            if z.day != local.date():
                z.day = local.date()
                z.on_seconds = 0.0
                z.weighted_seconds = 0.0
            pct = max(d.value for d in active)
            elapsed = (until - cursor).total_seconds()
            if pct > 0:
                z.on_seconds += elapsed
                z.weighted_seconds += elapsed * pct / 100
            cursor = until
        z.tick = when
        if z.day != self._day(when):
            z.day = self._day(when)
            z.on_seconds = 0.0
            z.weighted_seconds = 0.0

    def _event(self, at: datetime, kind: str, **fields) -> None:
        self.recent.append({"time": at.isoformat(), "kind": kind, **fields})
        del self.recent[:-MAX_RECENT]

    def observe(self, frame: Frame, at: datetime) -> bool:
        if at.tzinfo is None:
            raise ValueError("RF events require an aware timestamp")
        at = at.astimezone(timezone.utc)
        self.received += 1
        src = frame.source
        if frame.code == "3150" and frame.verb in ("I", "RP"):
            if (not src.startswith("04:") or self.controller_id not in frame.addresses[1:]
                    or len(frame.payload) != 2):
                return False
            zone_idx, raw = frame.payload
            pct = demand_pct(raw)
            if zone_idx >= MAX_ZONES or pct is None:
                return False
            z = self.zones.setdefault(zone_idx, ZoneState())
            # Snapshots advance the accounting cursor. A delayed RF report
            # must never retroactively replace already-counted demand time.
            # Reject it explicitly rather than silently overestimating.
            if ((z.last_seen is not None and at < z.last_seen)
                    or (z.tick is not None and at < z.tick)):
                self.ignored += 1
                return False
            self._advance(z, at)
            if src not in z.devices and len(z.devices) >= MAX_DEVICES_PER_ZONE:
                self.ignored += 1
                return False
            z.devices[src] = DeviceState(pct, at)
            z.last_seen = at
            z.samples += 1
            self._event(at, "zone_demand", zone=f"{zone_idx:02X}",
                        source=src, demand_pct=pct)
            return True
        # 0008 from the *controller* is a separate control signal, not measured
        # boiler fuel or verified firing. A 0008 originating from a thermostat
        # is NOT system/bridge demand.
        if (frame.code == "0008" and src == self.controller_id
                and frame.verb in ("I", "RP", "W")
                and len(frame.payload) == 2):
            pct = demand_pct(frame.payload[1])
            if pct is None:
                return False
            if self.controller_demand and at < self.controller_demand.seen:
                return False
            self.controller_demand = DeviceState(pct, at)
            self._event(at, "controller_0008", demand_pct=pct,
                        domain=f"{frame.payload[0]:02X}")
            return True
        # RX bridge exchanges; their presence is not equivalent to CH firing.
        if (frame.code in ("3220", "3EF0") and self.controller_id in frame.addresses
                and any(a.startswith("10:") for a in frame.addresses)):
            if self.bridge_last_seen is None or at >= self.bridge_last_seen:
                self.bridge_last_seen = at
            self.bridge_frames += 1
            return True
        return False

    def observe_mqtt(self, message: str | bytes, now: datetime | None = None) -> tuple[bool, int | None]:
        """Returns (accepted, newly-discovered-zone), never raises on untrusted MQTT."""
        now = (now or datetime.now(timezone.utc)).astimezone(timezone.utc)
        try:
            if isinstance(message, bytes):
                if len(message) > 2048:
                    return False, None
                message = message.decode("utf-8")
            if len(message) > 2048:
                return False, None
            data = json.loads(message)
            frame = parse_frame(data["msg"])
            stamped = datetime.fromisoformat(data["ts"].replace("Z", "+00:00"))
            if frame is None or stamped.tzinfo is None:
                return False, None
            at = stamped.astimezone(timezone.utc)
            # Ignore delayed/replayed and implausibly future packet timestamps.
            if at < now - timedelta(minutes=5) or at > now + timedelta(seconds=30):
                return False, None
            idx = (frame.payload[0] if frame.code == "3150"
                   and frame.payload and frame.source.startswith("04:") else None)
            is_new = idx is not None and idx not in self.zones
            accepted = self.observe(frame, at)
            return accepted, idx if accepted and is_new else None
        except (KeyError, ValueError, TypeError, AttributeError, UnicodeDecodeError):
            self.ignored += 1
            return False, None

    def _snapshot_zone(self, idx: int, z: ZoneState, now: datetime) -> dict:
        self._advance(z, now)
        value = self._current(z, now)
        devices = [{"id": key,
                    "demand_pct": d.value if d.seen <= now < d.seen + MAX_AGE else None,
                    "last_seen": d.seen.isoformat()}
                   for key, d in sorted(z.devices.items())]
        return {
            "index": f"{idx:02X}", "name": self.zone_names.get(idx, f"RF zone {idx:02X}"),
            "mapped": idx in self.zone_names, "demand_pct": value,
            "state": "unknown_stale" if value is None else "requesting" if value > 0 else "idle",
            "last_seen": z.last_seen.isoformat() if z.last_seen else None,
            "today_minutes": round(z.on_seconds / 60, 1),
            "today_weighted_minutes": round(z.weighted_seconds / 60, 1),
            "samples": z.samples, "devices": devices,
        }

    def snapshot(self, now: datetime | None = None) -> dict:
        now = (now or datetime.now(timezone.utc)).astimezone(timezone.utc)
        zones = [self._snapshot_zone(idx, z, now) for idx, z in sorted(self.zones.items())]
        controller = self.controller_demand
        fresh = controller and controller.seen <= now < controller.seen + MAX_AGE
        return {
            "enabled": True, "controller_id": self.controller_id,
            "date": self._day(now).isoformat(), "zones": zones,
            "controller_0008_pct": controller.value if fresh else None,
            "controller_0008_last_seen": controller.seen.isoformat() if controller else None,
            "bridge_last_seen": self.bridge_last_seen.isoformat() if self.bridge_last_seen else None,
            "bridge_frames": self.bridge_frames,
            "received": self.received, "ignored": self.ignored,
            "recent": list(reversed(self.recent)),
            "note": "Only verified RAMSES observations. Demand is not gas use or proof of boiler firing.",
        }

    def restore_daily(self, saved: dict, now: datetime | None = None) -> None:
        now = (now or datetime.now(timezone.utc)).astimezone(timezone.utc)
        if not isinstance(saved, dict) or saved.get("date") != self._day(now).isoformat():
            return
        for key, value in saved.get("zones", {}).items():
            try:
                idx = int(key, 16)
                minutes = float(value["minutes"])
                weighted = float(value["weighted"])
                if not 0 <= idx < MAX_ZONES or not 0 <= weighted <= minutes <= 1500:
                    continue
                self.zones[idx] = ZoneState(day=self._day(now),
                                             on_seconds=minutes * 60,
                                             weighted_seconds=weighted * 60,
                                             tick=now)
            except (ValueError, TypeError, KeyError, OverflowError):
                continue

    def daily_storage(self, now: datetime | None = None) -> dict:
        snap = self.snapshot(now)
        return {"date": snap["date"],
                "zones": {z["index"]: {"minutes": z["today_minutes"],
                                       "weighted": z["today_weighted_minutes"]}
                          for z in snap["zones"]}}
