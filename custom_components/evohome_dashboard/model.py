"""Cloud capabilities and runtime state converted to public, secret-free data."""
from __future__ import annotations

from datetime import datetime, timezone


def _iso(value):
    return value.isoformat() if hasattr(value, "isoformat") else (str(value) if value else None)


def _faults(faults):
    """Only return recognized fault metadata; never expose raw cloud payloads."""
    return [
        {"type": str(f.get("fault_type", "unknown"))[:80], "since": _iso(f.get("since"))}
        for f in faults if isinstance(f, dict)
    ]


def snapshot(client, *, updated: datetime | None = None) -> dict:
    locations = []
    for loc in client.locations:
        systems = []
        for gateway in loc.gateways:
            for system in gateway.systems:
                zones = []
                for zone in system.zones:
                    caps = zone.setpoint_capabilities
                    sched_caps = zone.schedule_capabilities or {}
                    zones.append({
                        "id": zone.id,
                        "name": zone.name,
                        "kind": str(zone.type),
                        "current": zone.temperature,
                        "target": zone.target_heat_temperature,
                        "mode": str(zone.mode),
                        "until": _iso(zone.until),
                        "minimum": zone.min_heat_setpoint,
                        "maximum": zone.max_heat_setpoint,
                        "step": caps.get("value_resolution", 0.5),
                        "allowed_modes": [str(m) for m in zone.allowed_modes],
                        "schedule_limits": {
                            "min_switchpoints_per_day": sched_caps.get("min_switchpoints_per_day", 1),
                            "max_switchpoints_per_day": sched_caps.get("max_switchpoints_per_day", 6),
                            "setpoint_value_resolution": sched_caps.get("setpoint_value_resolution", 0.5),
                            "timing_resolution": str(sched_caps.get("timing_resolution", "00:10:00")),
                        },
                        "faults": _faults(zone.active_faults),
                    })
                dhw = system.hotwater
                systems.append({
                    "id": system.id,
                    "model": str(system.model),
                    "mode": str(system.mode),
                    "until": _iso(system.until),
                    "allowed_modes": [str(m) for m in system.allowed_modes],
                    "faults": _faults(system.active_faults),
                    "zones": zones,
                    "hot_water": ({
                        "id": dhw.id, "state": str(dhw.state),
                        "mode": str(dhw.mode), "until": _iso(dhw.until),
                        "allowed_modes": [str(m) for m in dhw.allowed_modes],
                        "allowed_states": [str(m) for m in dhw.allowed_states],
                        "faults": _faults(dhw.active_faults),
                    } if dhw else None),
                })
        locations.append({"id": loc.id, "name": loc.name, "systems": systems})
    return {"updated": _iso(updated or datetime.now(timezone.utc)), "locations": locations}
