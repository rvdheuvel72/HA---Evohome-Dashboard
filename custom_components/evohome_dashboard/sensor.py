"""Entity-level transparency for every cloud-exposed heating-zone value."""
from homeassistant.components.sensor import SensorEntity, SensorDeviceClass
from homeassistant.helpers.update_coordinator import CoordinatorEntity
from homeassistant.helpers.dispatcher import async_dispatcher_connect
from homeassistant.components.sensor import SensorStateClass
from homeassistant.core import callback
from datetime import datetime
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
    monitor = getattr(hub, "rf_monitor", None)
    if monitor is not None:
        entities.extend([RFControllerDemand(monitor), RFBridgeActivity(monitor)])
        known = set(monitor.ledger.zones) | set(monitor.ledger.zone_names)
        for idx in sorted(known):
            entities.extend((RFZoneDemand(monitor, idx), RFZoneMinutes(monitor, idx)))
        # RF zones are learned after startup; never infer RF index from cloud
        # zone ordering. Each newly discovered index receives two entities.
        @callback
        def on_rf_event(new_idx):
            if new_idx is not None and new_idx not in known:
                known.add(new_idx)
                async_add_entities([RFZoneDemand(monitor, new_idx),
                                    RFZoneMinutes(monitor, new_idx)])

        entry.async_on_unload(async_dispatcher_connect(hass, monitor.signal, on_rf_event))
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


class _RFBase(SensorEntity):
    """Read-only local RAMSES measurements; independent of Resideo cloud polls."""

    _attr_has_entity_name = True
    _attr_should_poll = False

    def __init__(self, monitor, suffix, name):
        self.monitor = monitor
        self._attr_unique_id = f"{DOMAIN}_{monitor.entry_id}_rf_{suffix}"
        self._attr_name = name

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, f"rf_monitor_{self.monitor.entry_id}")},
                "name": "RAMSES RF demand monitor", "manufacturer": "Local RF observations"}

    @property
    def available(self):
        return self.monitor.connected

    async def async_added_to_hass(self):
        await super().async_added_to_hass()

        @callback
        def on_update(_):
            self.async_write_ha_state()

        self.async_on_remove(async_dispatcher_connect(
            self.hass, self.monitor.signal, on_update
        ))


class RFControllerDemand(_RFBase):
    _attr_native_unit_of_measurement = "%"
    _attr_state_class = SensorStateClass.MEASUREMENT

    def __init__(self, monitor):
        super().__init__(monitor, "controller_0008", "Controller RF demand (0008)")

    @property
    def native_value(self):
        return self.monitor.ledger.snapshot()["controller_0008_pct"]

    @property
    def extra_state_attributes(self):
        return {"last_seen": self.monitor.ledger.snapshot()["controller_0008_last_seen"],
                "note": "Observed 0008: not measured gas or proof the boiler fired"}


class RFBridgeActivity(_RFBase):
    _attr_device_class = SensorDeviceClass.TIMESTAMP

    def __init__(self, monitor):
        super().__init__(monitor, "bridge_last_seen", "OpenTherm bridge RF last seen")

    @property
    def native_value(self):
        value = self.monitor.ledger.snapshot()["bridge_last_seen"]
        return datetime.fromisoformat(value) if value else None


class _RFZoneBase(_RFBase):
    def __init__(self, monitor, index, suffix, label):
        self.index = index
        room = monitor.ledger.zone_names.get(index, f"RF zone {index:02X}")
        super().__init__(monitor, f"zone_{index:02x}_{suffix}", f"{room} {label}")

    def zone(self):
        for zone in self.monitor.ledger.snapshot()["zones"]:
            if zone["index"] == f"{self.index:02X}":
                return zone
        return None


class RFZoneDemand(_RFZoneBase):
    _attr_native_unit_of_measurement = "%"
    _attr_state_class = SensorStateClass.MEASUREMENT

    def __init__(self, monitor, index):
        super().__init__(monitor, index, "demand", "heat demand")

    @property
    def native_value(self):
        zone = self.zone()
        return zone["demand_pct"] if zone else None

    @property
    def extra_state_attributes(self):
        zone = self.zone()
        if zone is None:
            return {"zone_index": f"{self.index:02X}", "mapping_explicit": False}
        return {
            "zone_index": zone["index"], "mapping_explicit": zone["mapped"],
            "observation_state": zone["state"], "last_seen": zone["last_seen"],
            "today_request_minutes_est": zone["today_minutes"],
            "today_demand_weighted_minutes_est": zone["today_weighted_minutes"],
            "sample_count": zone["samples"], "trv_sources": zone["devices"],
        }


class RFZoneMinutes(_RFZoneBase):
    _attr_native_unit_of_measurement = "min"
    _attr_state_class = SensorStateClass.MEASUREMENT

    def __init__(self, monitor, index):
        super().__init__(monitor, index, "minutes", "estimated heat request today")

    @property
    def native_value(self):
        zone = self.zone()
        return zone["today_minutes"] if zone else 0
