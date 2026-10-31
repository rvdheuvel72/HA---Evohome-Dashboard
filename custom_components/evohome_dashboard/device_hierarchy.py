"""Register Evohome controller parent devices before zone entities.

Home Assistant 2026.8+ requires a registry device ID, not an identifiers tuple,
for DeviceInfo.via_device_id. Parent creation must precede entity forwarding.
"""
from __future__ import annotations

from homeassistant.helpers import device_registry as dr
from .const import DOMAIN


def register_system_devices(hass, entry_id: str, client) -> dict:
    """Return controller ID -> HA device registry ID for this config entry.

    async_get_or_create is idempotent for the existing identifier scoped to
    entry_id, so upgrades preserve the user's registered devices and areas.
    """
    registry = dr.async_get(hass)
    result = {}
    for location in client.locations:
        for gateway in location.gateways:
            for system in gateway.systems:
                device = registry.async_get_or_create(
                    config_entry_id=entry_id,
                    identifiers={(DOMAIN, f"system_{system.id}")},
                    name="Evohome heating controller",
                    manufacturer="Resideo",
                    model=str(system.model),
                )
                result[system.id] = device.id
    return result
