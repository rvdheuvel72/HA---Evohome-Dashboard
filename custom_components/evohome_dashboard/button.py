"""Manual follow-schedule actions for each zone."""
from homeassistant.components.button import ButtonEntity
from homeassistant.helpers.update_coordinator import CoordinatorEntity
from .const import DOMAIN


async def async_setup_entry(hass, entry, async_add_entities):
    hub = hass.data[DOMAIN][entry.entry_id]
    async_add_entities([
        ResumeZone(hub, system.id, zone.id)
        for loc in hub.client.locations for gateway in loc.gateways
        for system in gateway.systems for zone in system.zones
    ])


class ResumeZone(CoordinatorEntity, ButtonEntity):
    _attr_has_entity_name = True
    _attr_name = "Resume heating schedule"
    _attr_icon = "mdi:calendar-clock"

    def __init__(self, hub, system_id, zone_id):
        super().__init__(hub)
        self.system_id, self.zone_id = system_id, zone_id
        self._attr_unique_id = f"{DOMAIN}_{system_id}_{zone_id}_resume_schedule"

    @property
    def device_info(self):
        zone = self.coordinator._zone(self.system_id, self.zone_id)
        return {"identifiers": {(DOMAIN, f"zone_{self.zone_id}")},
                "via_device_id": self.coordinator.system_device_ids[self.system_id],
                "name": zone.name}

    async def async_press(self):
        await self.coordinator.resume_zone(self.system_id, self.zone_id)
