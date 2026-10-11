"""System mode and DHW schedule/override controls."""
from __future__ import annotations

from homeassistant.components.select import SelectEntity
from homeassistant.helpers.update_coordinator import CoordinatorEntity
from .const import DOMAIN

HOTWATER_OPTIONS = {
    "Follow schedule": "follow_schedule",
    "On (until changed)": "on",
    "Off (until changed)": "off",
}


async def async_setup_entry(hass, entry, async_add_entities):
    hub = hass.data[DOMAIN][entry.entry_id]
    result = []
    for loc in hub.client.locations:
        for gateway in loc.gateways:
            for system in gateway.systems:
                result.append(SystemModeSelect(hub, system.id))
                if system.hotwater:
                    result.append(HotWaterSelect(hub, system.id))
    async_add_entities(result)


class SystemModeSelect(CoordinatorEntity, SelectEntity):
    _attr_has_entity_name = True
    _attr_name = "Operating mode"
    _attr_icon = "mdi:home-thermometer-outline"

    def __init__(self, hub, system_id):
        super().__init__(hub)
        self.system_id = system_id
        self._attr_unique_id = f"{DOMAIN}_{system_id}_system_mode"

    @property
    def device_info(self):
        s = self.coordinator._system(self.system_id)
        return {
            "identifiers": {(DOMAIN, f"system_{self.system_id}")},
            "name": "Evohome heating controller",
            "manufacturer": "Resideo",
            "model": str(s.model),
        }

    @property
    def options(self):
        return [str(m) for m in self.coordinator._system(self.system_id).allowed_modes]

    @property
    def current_option(self):
        return str(self.coordinator._system(self.system_id).mode)

    @property
    def extra_state_attributes(self):
        system = self.coordinator._system(self.system_id)
        return {
            "override_until": system.until.isoformat() if system.until else None,
            "system_id": self.system_id,
            "full_heating_off_supported": "heating_off" in self.options,
        }

    async def async_select_option(self, option):
        if option not in self.options:
            raise ValueError("Unsupported system mode")
        await self.coordinator.set_system_mode(self.system_id, option)


class HotWaterSelect(CoordinatorEntity, SelectEntity):
    _attr_has_entity_name = True
    _attr_name = "Hot water control"
    _attr_icon = "mdi:water-boiler"

    def __init__(self, hub, system_id):
        super().__init__(hub)
        self.system_id = system_id
        self._attr_unique_id = f"{DOMAIN}_{system_id}_hotwater_control"

    @property
    def device_info(self):
        return {
            "identifiers": {(DOMAIN, f"system_{self.system_id}")},
            "name": "Evohome heating controller",
        }

    @property
    def options(self):
        return list(HOTWATER_OPTIONS)

    @property
    def current_option(self):
        dhw = self.coordinator._system(self.system_id).hotwater
        if not dhw:
            return None
        if str(dhw.mode) == "follow_schedule":
            return "Follow schedule"
        return "On (until changed)" if str(dhw.state) == "on" else "Off (until changed)"

    async def async_select_option(self, option):
        if option not in self.options:
            raise ValueError("Unsupported hot water option")
        state = HOTWATER_OPTIONS[option]
        if state == "follow_schedule":
            await self.coordinator.resume_hotwater(self.system_id)
        else:
            await self.coordinator.set_hotwater(self.system_id, state)
