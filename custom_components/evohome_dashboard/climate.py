"""Native HA climate entities, one per Evohome heating zone."""
from __future__ import annotations

from datetime import datetime, timedelta, timezone

from homeassistant.components.climate import ClimateEntity, ClimateEntityFeature, HVACMode
from homeassistant.helpers.update_coordinator import CoordinatorEntity


async def async_setup_entry(hass, entry, async_add_entities):
    hub = hass.data["evohome_dashboard"][entry.entry_id]
    async_add_entities([
        EvoZone(hub, system.id, zone.id)
        for loc in hub.client.locations
        for gateway in loc.gateways
        for system in gateway.systems
        for zone in system.zones
    ])


class EvoZone(CoordinatorEntity, ClimateEntity):
    _attr_temperature_unit = "°C"
    _attr_hvac_modes = [HVACMode.HEAT]
    _attr_supported_features = ClimateEntityFeature.TARGET_TEMPERATURE
    _attr_has_entity_name = True

    def __init__(self, hub, system_id, zone_id):
        super().__init__(hub)
        self._system_id = system_id
        self._zone_id = zone_id
        self._attr_unique_id = f"evohome_dashboard_{system_id}_{zone_id}"
        self._attr_name = None

    @property
    def _zone(self):
        return self.coordinator._zone(self._system_id, self._zone_id)

    @property
    def device_info(self):
        return {
            "identifiers": {("evohome_dashboard", self._system_id)},
            "name": f"Evohome {self._system_id}",
            "manufacturer": "Resideo",
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
        return HVACMode.HEAT

    @property
    def extra_state_attributes(self):
        zone = self._zone
        return {
            "zone_id": zone.id, "system_id": self._system_id,
            "setpoint_mode": str(zone.mode),
            "override_until": zone.until.isoformat() if zone.until else None,
        }

    async def async_set_temperature(self, **kwargs):
        temp = kwargs.get("temperature")
        if temp is None:
            return
        await self.coordinator.set_zone(self._system_id, self._zone_id, temp)
