"""Entity-level transparency for every cloud-exposed heating-zone value."""
from homeassistant.components.sensor import SensorEntity, SensorDeviceClass
from homeassistant.helpers.update_coordinator import CoordinatorEntity
from .const import DOMAIN


async def async_setup_entry(hass, entry, async_add_entities):
    hub = hass.data[DOMAIN][entry.entry_id]
    entities = [EvohomeHealth(hub)]
    for loc in hub.client.locations:
        for gateway in loc.gateways:
            for sys in gateway.systems:
                entities.append(EvoModeSensor(hub, sys.id, None, "system_mode"))
                if sys.hotwater:
                    entities.append(EvoModeSensor(hub, sys.id, None, "hotwater_state"))
                for zone in sys.zones:
                    for kind in ("current_temperature", "target_temperature",
                                 "override_mode", "override_until", "zone_configuration"):
                        entities.append(EvoModeSensor(hub, sys.id, zone.id, kind))
    async_add_entities(entities)


class EvohomeHealth(CoordinatorEntity, SensorEntity):
    _attr_has_entity_name = True
    _attr_name = "Cloud connection"
    _attr_icon = "mdi:cloud-check"

    def __init__(self, hub):
        super().__init__(hub)
        self._attr_unique_id = f"{DOMAIN}_{hub.entry_id}_cloud_health"

    @property
    def native_value(self):
        return "Connected" if self.coordinator.last_update_success else "Unavailable"

    @property
    def extra_state_attributes(self):
        data = self.coordinator.data or {}
        return {"last_success": data.get("updated"),
                "error_count": len(self.coordinator.error_data()),
                "cloud_dependency": True,
                "dashboard": "/evohome-dashboard"}


class EvoModeSensor(CoordinatorEntity, SensorEntity):
    _attr_has_entity_name = True
    _attr_entity_registry_enabled_default = True

    def __init__(self, hub, system_id, zone_id, kind):
        super().__init__(hub)
        self.system_id, self.zone_id, self.kind = system_id, zone_id, kind
        self._attr_unique_id = f"{DOMAIN}_{system_id}_{zone_id or 'controller'}_{kind}"
        self._attr_name = kind.replace("_", " ").capitalize()
        if kind in ("current_temperature", "target_temperature"):
            self._attr_native_unit_of_measurement = "°C"
            self._attr_device_class = SensorDeviceClass.TEMPERATURE
        if kind == "override_until":
            self._attr_device_class = SensorDeviceClass.TIMESTAMP

    @property
    def device_info(self):
        if self.zone_id:
            zone = self.coordinator._zone(self.system_id, self.zone_id)
            return {"identifiers": {(DOMAIN, f"zone_{self.zone_id}")},
                    "via_device": (DOMAIN, f"system_{self.system_id}"),
                    "name": zone.name}
        return {"identifiers": {(DOMAIN, f"system_{self.system_id}")},
                "name": "Evohome heating controller"}

    @property
    def _zone(self):
        return self.coordinator._zone(self.system_id, self.zone_id) if self.zone_id else None

    @property
    def native_value(self):
        z = self._zone
        if self.kind == "system_mode":
            return str(self.coordinator._system(self.system_id).mode)
        if self.kind == "hotwater_state":
            dhw = self.coordinator._system(self.system_id).hotwater
            return str(dhw.state) if dhw else None
        if self.kind == "current_temperature":
            return z.temperature
        if self.kind == "target_temperature":
            return z.target_heat_temperature
        if self.kind == "override_mode":
            return str(z.mode)
        if self.kind == "override_until":
            return z.until
        if self.kind == "zone_configuration":
            return str(z.type)
        return None

    @property
    def extra_state_attributes(self):
        z = self._zone
        if self.kind == "zone_configuration" and z:
            return {
                "model": str(z.model), "zone_id": z.id,
                "system_id": self.system_id,
                "setpoint_min": z.min_heat_setpoint,
                "setpoint_max": z.max_heat_setpoint,
                "setpoint_resolution": z.setpoint_capabilities.get("value_resolution"),
                "allowed_modes": [str(v) for v in z.allowed_modes],
                "schedule_capabilities": {str(k): str(v)
                                          for k,v in (z.schedule_capabilities or {}).items()},
                "schedule_editor": "/evohome-dashboard",
                "note": "Installer RF/binding settings not exposed by Resideo TCC",
            }
        return {}
