"""Pure schedule validation and conflict detection. Vendor schedule keys are snake_case."""
from __future__ import annotations

import hashlib
import json
from datetime import time

DAYS = ("monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday")


def digest(schedule: list[dict]) -> str:
    """Deterministic fingerprint, never containing authentication information."""
    return hashlib.sha256(json.dumps(schedule, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def validate(schedule: object, *, limits: dict | None = None) -> list[dict]:
    """Strictly validate week, time ordering, setpoints and device constraints."""
    if not isinstance(schedule, list) or len(schedule) != 7:
        raise ValueError("The schedule must contain all seven days")
    limits = limits or {}
    min_points = int(limits.get("min_switchpoints_per_day", 1))
    max_points = int(limits.get("max_switchpoints_per_day", 6))
    resolution = float(limits.get("setpoint_value_resolution", 0.5))
    result = []
    seen = set()
    raw_resolution = str(limits.get("timing_resolution", "00:10:00"))
    chunks = raw_resolution.split(":")
    tick_seconds = (int(chunks[-3]) * 3600 + int(chunks[-2]) * 60 + int(chunks[-1])) if len(chunks) >= 3 else 600
    if tick_seconds <= 0:
        raise ValueError("Invalid controller time resolution")
    for day in schedule:
        if not isinstance(day, dict) or set(day) != {"day_of_week", "switchpoints"}:
            raise ValueError("Invalid day format")
        name = day["day_of_week"]
        if name not in DAYS or name in seen:
            raise ValueError("Days must be unique and valid")
        seen.add(name)
        points = day["switchpoints"]
        if not isinstance(points, list) or not min_points <= len(points) <= max_points:
            raise ValueError("Invalid number of switchpoints")
        sorted_points = []
        previous = None
        for point in points:
            if not isinstance(point, dict) or set(point) != {"time_of_day", "heat_setpoint"}:
                raise ValueError("Invalid switchpoint format")
            value = str(point["time_of_day"])
            try:
                moment = time.fromisoformat(value)
            except ValueError as err:
                raise ValueError("Invalid time") from err
            if moment.second or moment.microsecond or ((moment.hour * 3600 + moment.minute * 60) % tick_seconds):
                raise ValueError("Switchpoint time violates controller timing resolution")
            if previous is not None and moment <= previous:
                raise ValueError("Switchpoints must be strictly ascending")
            previous = moment
            temp = point["heat_setpoint"]
            if isinstance(temp, bool) or not isinstance(temp, (int, float)):
                raise ValueError("Temperature must be numeric")
            temp = float(temp)
            if not 5 <= temp <= 35 or abs(round(temp / resolution) * resolution - temp) > 1e-6:
                raise ValueError("Setpoint outside valid range or resolution")
            sorted_points.append({"time_of_day": moment.strftime("%H:%M:%S"), "heat_setpoint": temp})
        result.append({"day_of_week": name, "switchpoints": sorted_points})
    return sorted(result, key=lambda item: DAYS.index(item["day_of_week"]))


def normalize(schedule: list[dict]) -> list[dict]:
    """Convert known snake/camel-case schedule variants to a single public shape."""
    result = []
    for day in schedule:
        points = day.get("switchpoints", [])
        result.append({
            "day_of_week": str(day.get("day_of_week", day.get("dayOfWeek", ""))).lower(),
            "switchpoints": [{
                "time_of_day": p.get("time_of_day", p.get("timeOfDay")),
                "heat_setpoint": p.get("heat_setpoint", p.get("heatSetpoint")),
            } for p in points],
        })
    return result
