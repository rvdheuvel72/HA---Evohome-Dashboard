"""Fault indicators using only live cloud reported active faults."""
from homeassistant.components.binary_sensor import BinarySensorEntity, BinarySensorDeviceClass
from homeassistant.helpers.update_coordinator import CoordinatorEntity
from .const import DOMAIN


async def async_setup_entry(hass, entry, async_add_entities):
    hub = hass.data[DOMAIN][entry.entry_id]
    entities = []
    for loc in hub.client.locations:
        for gateway in loc.gateways:
            for sys in gateway.systems:
                entities.append(EvoFaults(hub, sys.id, None))
                entities.extend(EvoFaults(hub, sys.id, zone.id) for zone in sys.zones)
    async_add_entities(entities)


class EvoFaults(CoordinatorEntity, BinarySensorEntity):
    _attr_has_entity_name = True
    _attr_name = "Active fault"
    _attr_device_class = BinarySensorDeviceClass.PROBLEM

    def __init__(self, hub, system_id, zone_id):
        super().__init__(hub)
        self.system_id, self.zone_id = system_id, zone_id
        self._attr_unique_id = f"{DOMAIN}_{system_id}_{zone_id or 'controller'}_fault"

    @property
    def _device(self):
        return (self.coordinator._zone(self.system_id, self.zone_id)
                if self.zone_id else self.coordinator._system(self.system_id))

    @property
    def device_info(self):
        if self.zone_id:
            return {"identifiers": {(DOMAIN, f"zone_{self.zone_id}")}}
        return {"identifiers": {(DOMAIN, f"system_{self.system_id}")},
                "name": "Evohome heating controller"}

    @property
    def is_on(self):
        return bool(self._device.active_faults)

    @property
    def extra_state_attributes(self):
        # No secret-bearing raw fault payloads.
        return {"fault_count": len(self._device.active_faults),
                "fault_types": [str(x.get("fault_type","unknown"))[:80]
                                for x in self._device.active_faults if isinstance(x,dict)]}
