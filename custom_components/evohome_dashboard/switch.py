"""Cloud heating switches; zones OFF retain frost protection."""
from __future__ import annotations

from homeassistant.components.switch import SwitchEntity
from homeassistant.helpers.update_coordinator import CoordinatorEntity
from .const import DOMAIN


async def async_setup_entry(hass, entry, async_add_entities):
    hub = hass.data[DOMAIN][entry.entry_id]
    result = []
    for loc in hub.client.locations:
        for gateway in loc.gateways:
            for sys in gateway.systems:
                if "heating_off" in [str(x) for x in sys.allowed_modes]:
                    result.append(SystemHeating(hub, sys.id))
                for zone in sys.zones:
                    result.append(ZoneHeating(hub, sys.id, zone.id))
    async_add_entities(result)


class SystemHeating(CoordinatorEntity, SwitchEntity):
    _attr_has_entity_name = True
    _attr_name = "Heating enabled"
    _attr_icon = "mdi:radiator"

    def __init__(self, hub, system_id):
        super().__init__(hub)
        self.system_id = system_id
        self._attr_unique_id = f"{DOMAIN}_{system_id}_heating_enabled"

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, f"system_{self.system_id}")},
                "name": "Evohome heating controller"}

    @property
    def is_on(self):
        return str(self.coordinator._system(self.system_id).mode) != "heating_off"

    @property
    def extra_state_attributes(self):
        return {"on_restores": "auto", "note": "Heating Off applies to the full Evohome system"}

    async def async_turn_off(self, **kwargs):
        await self.coordinator.set_system_mode(self.system_id, "heating_off")

    async def async_turn_on(self, **kwargs):
        await self.coordinator.set_system_mode(self.system_id, "auto")


class ZoneHeating(CoordinatorEntity, SwitchEntity):
    _attr_has_entity_name = True
    _attr_name = "Heating enabled (frost protected)"
    _attr_icon = "mdi:radiator"

    def __init__(self, hub, system_id, zone_id):
        super().__init__(hub)
        self.system_id, self.zone_id = system_id, zone_id
        self._attr_unique_id = f"{DOMAIN}_{system_id}_{zone_id}_zone_enabled"

    @property
    def device_info(self):
        zone = self.coordinator._zone(self.system_id, self.zone_id)
        return {"identifiers": {(DOMAIN, f"zone_{self.zone_id}")},
                "via_device": (DOMAIN, f"system_{self.system_id}"),
                "name": zone.name}

    @property
    def is_on(self):
        z = self.coordinator._zone(self.system_id, self.zone_id)
        return not (
            str(z.mode) == "permanent_override"
            and abs(z.target_heat_temperature - z.min_heat_setpoint) < 1e-3
        )

    @property
    def extra_state_attributes(self):
        return {
            "off_meaning": "Minimum setpoint / frost protection, NOT RF device power-off",
            "resume_when_on": "follow_schedule",
        }

    async def async_turn_off(self, **kwargs):
        await self.coordinator.set_zone_frost(self.system_id, self.zone_id)

    async def async_turn_on(self, **kwargs):
        await self.coordinator.resume_zone(self.system_id, self.zone_id)
