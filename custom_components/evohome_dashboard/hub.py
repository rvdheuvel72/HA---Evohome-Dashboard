"""Independent orchestration for Home Assistant's evohome-async==2.1.0 client.

The v2 client loads its configuration via EvohomeClient.update(), polls
location status via Location.update(), and its write methods return None.
We verify state with new cloud status fetches instead of relying on PUT ACK.
"""
from __future__ import annotations

import asyncio
import logging
from collections import deque
from datetime import datetime, timedelta, timezone

from evohomeasync2 import EvohomeClient
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed
from homeassistant.helpers.storage import Store
from .model import snapshot
from .schedules import digest, normalize, validate
from .const import DOMAIN, MAX_ERRORS
from .verification import matches_override_expiry

_LOGGER = logging.getLogger(__name__)


def utc_now():
    return datetime.now(timezone.utc)


def parse_until(value):
    if value is None:
        return None
    moment = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if moment.tzinfo is None:
        raise ValueError("An explicit timezone is required")
    if not utc_now() < moment <= utc_now() + timedelta(hours=24):
        raise ValueError("Override expiry must be within the next 24 hours")
    return moment


class EvohomeHub(DataUpdateCoordinator[dict]):
    """One location tree per account and serialized control requests."""

    # A Resideo PUT can be accepted before it is applied by the physical controller.
    VERIFY_ATTEMPTS = 7
    VERIFY_INTERVAL = 5.0

    def __init__(self, hass, entry_id, token_manager, interval):
        super().__init__(
            hass, _LOGGER, name=f"{DOMAIN}_{entry_id}",
            update_interval=timedelta(seconds=interval),
        )
        self.client = EvohomeClient(token_manager)
        self.entry_id = entry_id
        self.system_device_ids = {}  # HA registry IDs, scoped to this config entry
        self._write_lock = asyncio.Lock()
        self._read_lock = asyncio.Lock()
        self._errors = deque(maxlen=MAX_ERRORS)
        self._error_store = Store(hass, 1, f"evohome_dashboard_errors_{entry_id}", private=True)
        self._schedule_cache = {}

    async def initialize(self):
        events = await self._error_store.async_load()
        for event in (events or {}).get("events", []):
            if isinstance(event, dict):
                self._errors.append(event)
        await self.client.update(dont_update_status=True)

    async def _async_update_data(self):
        try:
            # Fetch status only; schedules are loaded on demand.
            await asyncio.gather(*(loc.update() for loc in self.client.locations))
            return snapshot(self.client)
        except Exception as exc:
            await self._record("poll_status", exc)
            raise UpdateFailed("Unable to update Evohome temperatures") from exc

    async def _record(self, action, exception, target=None):
        event = {
            "time": utc_now().isoformat(),
            "operation": action,
            "category": type(exception).__name__,
            "target": str(target or "")[:40],
        }
        self._errors.appendleft(event)
        # Do not log vendor error text: it may contain sensitive request details.
        _LOGGER.error("Evohome %s failed (%s)", action, type(exception).__name__)
        try:
            await self._error_store.async_save({"events": list(self._errors)})
        except Exception:
            _LOGGER.error("Could not persist Evohome error log")

    def error_data(self):
        return list(self._errors)

    def _system(self, system_id):
        for location in self.client.locations:
            for gateway in location.gateways:
                if system_id in gateway.system_by_id:
                    return gateway.system_by_id[system_id]
        raise ValueError("Unknown system")

    def _zone(self, system_id, zone_id):
        system = self._system(system_id)
        if zone_id not in system.zone_by_id:
            raise ValueError("Unknown zone")
        return system.zone_by_id[zone_id]

    async def _wait_for_state(self, location, predicate):
        """Poll the physical device's reported state after accepted cloud PUT.

        This is a state observation, not direct acknowledgement from device RF.
        On failure, never tell the dashboard the command succeeded.
        """
        for attempt in range(self.VERIFY_ATTEMPTS):
            if attempt:
                await asyncio.sleep(self.VERIFY_INTERVAL)
            await location.update()
            if predicate():
                return
        raise TimeoutError("The requested heating state was not confirmed by Resideo")

    async def _execute(self, operation, target, action, location, predicate, *, previous_expiry=None):
        """Serialize the cloud PUT and verify a fresh observed status after it."""
        async with self._write_lock:
            try:
                # Prior state must be sampled under the same lock as the PUT.
                prior = previous_expiry() if previous_expiry is not None else None
                # v2.1.0 PUT methods return None, not v3 CommTask objects.
                async with asyncio.timeout(60):
                    await action()
                validator = (lambda: predicate(prior)) if previous_expiry is not None else predicate
                await self._wait_for_state(location, validator)
                await self.async_request_refresh()
                return {"ok": True, "verified": True}
            except Exception as exc:
                await self._record(operation, exc, target)
                raise

    async def set_zone(self, system_id, zone_id, temperature, until=None):
        zone = self._zone(system_id, zone_id)
        value = float(temperature)
        step = float(zone.setpoint_capabilities.get("value_resolution", 0.5))
        if step <= 0 or not zone.min_heat_setpoint <= value <= zone.max_heat_setpoint:
            raise ValueError("Temperature out of range")
        if abs((value / step) - round(value / step)) > 1e-6:
            raise ValueError("Unsupported temperature step")
        expiry = parse_until(until)
        expected_mode = "temporary_override" if expiry else "permanent_override"
        return await self._execute(
            "zone_temperature", zone_id,
            lambda: zone.set_temperature(value, until=expiry), zone.location,
            lambda prior: str(zone.mode) == expected_mode
            and abs(zone.target_heat_temperature - value) <= 1e-6
            and matches_override_expiry(zone.until, expiry, previous=prior),
            previous_expiry=lambda: zone.until,
        )

    async def set_zone_frost(self, system_id, zone_id):
        """Zone off is minimum-temperature frost protection, not RF disconnect."""
        zone = self._zone(system_id, zone_id)
        return await self.set_zone(system_id, zone_id, zone.min_heat_setpoint)

    async def resume_zone(self, system_id, zone_id):
        zone = self._zone(system_id, zone_id)
        return await self._execute(
            "zone_resume", zone_id, zone.reset, zone.location,
            lambda: str(zone.mode) == "follow_schedule",
        )

    async def set_system_mode(self, system_id, mode, until=None):
        system = self._system(system_id)
        if mode not in [str(item) for item in system.allowed_modes]:
            raise ValueError("System mode unsupported")
        expiry = parse_until(until)
        return await self._execute(
            "system_mode", system_id,
            lambda: system.set_mode(mode, until=expiry), system.location,
            lambda prior: str(system.mode) == mode
            and matches_override_expiry(system.until, expiry, previous=prior),
            previous_expiry=lambda: system.until,
        )

    async def set_hotwater(self, system_id, state, until=None):
        system = self._system(system_id)
        dhw = system.hotwater
        if dhw is None or state not in ("on", "off"):
            raise ValueError("Hot water unavailable or invalid state")
        expiry = parse_until(until)
        expected_mode = "temporary_override" if expiry else "permanent_override"
        return await self._execute(
            "hotwater", system_id,
            lambda: dhw.set_state(state, until=expiry), system.location,
            lambda prior: str(dhw.state) == state and str(dhw.mode) == expected_mode
            and matches_override_expiry(dhw.until, expiry, previous=prior),
            previous_expiry=lambda: dhw.until,
        )

    async def resume_hotwater(self, system_id):
        system = self._system(system_id)
        dhw = system.hotwater
        if not dhw:
            raise ValueError("No hot water device")
        return await self._execute(
            "hotwater_resume", system_id, dhw.reset, system.location,
            lambda: str(dhw.mode) == "follow_schedule",
        )

    async def get_zone_schedule(self, system_id, zone_id, *, force=False):
        zone = self._zone(system_id, zone_id)
        key = f"{system_id}:{zone_id}"
        async with self._read_lock:
            cached = self._schedule_cache.get(key)
            if not force and cached and cached["expires"] > utc_now():
                return cached["payload"]
            try:
                data = normalize(await zone.get_schedule())
                payload = {
                    "system_id": system_id, "zone_id": zone_id, "schedule": data,
                    "version": digest(data), "limits": snapshot_limits(zone),
                }
                self._schedule_cache[key] = {
                    "expires": utc_now() + timedelta(minutes=15), "payload": payload,
                }
                return payload
            except Exception as exc:
                await self._record("get_schedule", exc, zone_id)
                raise

    async def save_zone_schedule(self, system_id, zone_id, schedule, expected):
        zone = self._zone(system_id, zone_id)
        async with self._write_lock:
            try:
                latest = normalize(await zone.get_schedule())
                if digest(latest) != expected:
                    raise ValueError("Schedule changed elsewhere; reload before saving")
                validated = validate(schedule, limits=snapshot_limits(zone))
                if digest(validated) == expected:
                    return {"ok": True, "unchanged": True}
                # v2.1.0 PUT acknowledges without a CommTask; verify via GET.
                async with asyncio.timeout(60):
                    await zone.set_schedule(validated)
                # Confirm with repeated live GETs, not only task completion.
                for attempt in range(self.VERIFY_ATTEMPTS):
                    if attempt:
                        await asyncio.sleep(self.VERIFY_INTERVAL)
                    confirmed = normalize(await zone.get_schedule())
                    if digest(confirmed) == digest(validated):
                        self._schedule_cache.pop(f"{system_id}:{zone_id}", None)
                        return {"ok": True, "verified": True, "version": digest(confirmed)}
                raise TimeoutError("New schedule was not confirmed by Resideo")
            except Exception as exc:
                await self._record("save_schedule", exc, zone_id)
                raise


def snapshot_limits(zone):
    caps = zone.schedule_capabilities or {}
    return {
        "min_heat_setpoint": zone.min_heat_setpoint,
        "max_heat_setpoint": zone.max_heat_setpoint,
        "min_switchpoints_per_day": caps.get("min_switchpoints_per_day", 1),
        "max_switchpoints_per_day": caps.get("max_switchpoints_per_day", 6),
        "setpoint_value_resolution": caps.get("setpoint_value_resolution", 0.5),
        "timing_resolution": str(caps.get("timing_resolution", "00:10:00")),
    }
