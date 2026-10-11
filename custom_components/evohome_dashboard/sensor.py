"""Diagnostic health sensor; detailed error history is exposed to administrators only."""
from homeassistant.components.sensor import SensorEntity
from homeassistant.helpers.update_coordinator import CoordinatorEntity


async def async_setup_entry(hass, entry, async_add_entities):
    async_add_entities([EvohomeHealth(hass.data["evohome_dashboard"][entry.entry_id])])


class EvohomeHealth(CoordinatorEntity, SensorEntity):
    _attr_has_entity_name = True
    _attr_name = "Cloud connection"
    _attr_icon = "mdi:cloud-check"

    def __init__(self, hub):
        super().__init__(hub)
        self._attr_unique_id = f"evohome_dashboard_{hub.entry_id}_cloud_health"

    @property
    def native_value(self):
        return "Connected" if self.coordinator.last_update_success else "Unavailable"

    @property
    def extra_state_attributes(self):
        data = self.coordinator.data or {}
        return {
            "last_success": data.get("updated"),
            "error_count": len(self.coordinator.error_data()),
            "cloud_dependency": True,
        }
