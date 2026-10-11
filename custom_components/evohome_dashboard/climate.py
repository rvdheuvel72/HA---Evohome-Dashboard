"""Native HA zone thermostats including safe frost-protection semantics."""
from __future__ import annotations

from datetime import datetime, timedelta, timezone
from homeassistant.components.climate import ClimateEntity, ClimateEntityFeature, HVACMode
from homeassistant.helpers.update_coordinator import CoordinatorEntity
from .const import DOMAIN

FROST_EPSILON = 0.001


async def async_setup_entry(hass, entry, async_add_entities):
    hub = hass.data[DOMAIN][entry.entry_id]
    async_add_entities([
        EvoZone(hub, system.id, zone.id)
        for loc in hub.client.locations
        for gateway in loc.gateways
        for system in gateway.systems
        for zone in system.zones
    ])


class EvoZone(CoordinatorEntity, ClimateEntity):
    """OFF is a minimum-temperature override (frost protected), not a power cut."""

    _attr_temperature_unit = "°C"
    _attr_hvac_modes = [HVACMode.HEAT, HVACMode.OFF]
    _attr_supported_features = ClimateEntityFeature.TARGET_TEMPERATURE | ClimateEntityFeature.PRESET_MODE
    _attr_preset_modes = ["Follow schedule", "Temporary override", "Permanent override", "Frost protection"]
    _attr_has_entity_name = True

    def __init__(self, hub, system_id, zone_id):
        super().__init__(hub)
        self._system_id, self._zone_id = system_id, zone_id
        self._attr_unique_id = f"{DOMAIN}_{system_id}_{zone_id}"
        self._attr_name = None

    @property
    def _zone(self):
        return self.coordinator._zone(self._system_id, self._zone_id)

    @property
    def device_info(self):
        return {
            "identifiers": {(DOMAIN, f"zone_{self._zone_id}")},
            "via_device": (DOMAIN, f"system_{self._system_id}"),
            "name": self._zone.name,
            "manufacturer": "Resideo",
            "model": str(self._zone.model),
        }

    @property
    def name(self):
        return self._zone.name

    @property
    def available(self):
        return self.coordinator.last_update_success

    @property
    def current_temperature(self):
        return self._zone.temperature

    @property
    def target_temperature(self):
        return self._zone.target_heat_temperature

    @property
    def min_temp(self):
        return self._zone.min_heat_setpoint

    @property
    def max_temp(self):
        return self._zone.max_heat_setpoint

    @property
    def target_temperature_step(self):
        return float(self._zone.setpoint_capabilities.get("value_resolution", 0.5))

    @property
    def hvac_mode(self):
        z = self._zone
        if str(z.mode) == "permanent_override" and abs(
            z.target_heat_temperature - z.min_heat_setpoint
        ) < FROST_EPSILON:
            return HVACMode.OFF
        return HVACMode.HEAT

    @property
    def preset_mode(self):
        z = self._zone
        if self.hvac_mode == HVACMode.OFF:
            return "Frost protection"
        if str(z.mode) == "follow_schedule":
            return "Follow schedule"
        if str(z.mode) == "temporary_override":
            return "Temporary override"
        return "Permanent override"

    async def async_set_preset_mode(self, preset_mode):
        if preset_mode == "Follow schedule":
            await self.coordinator.resume_zone(self._system_id, self._zone_id)
        elif preset_mode == "Frost protection":
            await self.coordinator.set_zone_frost(self._system_id, self._zone_id)
        elif preset_mode == "Temporary override":
            await self.coordinator.set_zone(self._system_id, self._zone_id,
                                            self._zone.target_heat_temperature,
                                            until=(datetime.now(timezone.utc) + timedelta(hours=1)).isoformat())
        elif preset_mode == "Permanent override":
            await self.coordinator.set_zone(self._system_id, self._zone_id,
                                            self._zone.target_heat_temperature)
        else:
            raise ValueError("Unknown preset")

    @property
    def extra_state_attributes(self):
        z = self._zone
        return {
            "zone_id": z.id,
            "system_id": self._system_id,
            "setpoint_mode": str(z.mode),
            "override_until": z.until.isoformat() if z.until else None,
            "allowed_modes": [str(v) for v in z.allowed_modes],
            "frost_protection_off": True,
            "frost_protection_temperature": z.min_heat_setpoint,
            "minimum_setpoint": z.min_heat_setpoint,
            "maximum_setpoint": z.max_heat_setpoint,
            "schedule_editor": "/evohome-dashboard",
        }

    async def async_set_temperature(self, **kwargs):
        target = kwargs.get("temperature")
        if target is not None:
            await self.coordinator.set_zone(self._system_id, self._zone_id, target)

    async def async_set_hvac_mode(self, hvac_mode):
        if hvac_mode == HVACMode.OFF:
            await self.coordinator.set_zone_frost(self._system_id, self._zone_id)
        elif hvac_mode == HVACMode.HEAT:
            await self.coordinator.resume_zone(self._system_id, self._zone_id)
        else:
            raise ValueError("Unsupported zone HVAC mode")
