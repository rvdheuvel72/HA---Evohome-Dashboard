"""Evohome Dashboard: cloud-native Evohome integration and touchscreen panel."""
from __future__ import annotations

import json
import logging
from pathlib import Path

from homeassistant.components import frontend, panel_custom
from homeassistant.components.http import StaticPathConfig
from homeassistant.const import CONF_PASSWORD, CONF_SCAN_INTERVAL, CONF_USERNAME
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from .auth import StoredTokenManager, token_cache_key
from .const import (
    DEFAULT_INTERVAL, DOMAIN, PANEL_PATH,
    CONF_RF_CONTROLLER_ID, CONF_RF_MQTT_TOPIC, CONF_RF_ZONE_NAMES,
    DEFAULT_RF_MQTT_TOPIC,
)
from .heat_demand import valid_zone_names
from .hub import EvohomeHub
from .device_hierarchy import register_system_devices
from .websocket import register_commands

_LOGGER = logging.getLogger(__name__)
PLATFORMS = ["climate","sensor","binary_sensor","select","switch","button"]


async def async_setup(hass, config):
    hass.data.setdefault(DOMAIN, {})
    return True


async def _setup_frontend(hass):
    if hass.data.get(f"{DOMAIN}_ui_loaded"):
        return
    if not hass.data.get(f"{DOMAIN}_assets_loaded"):
        assets = Path(__file__).parent / "frontend"
        await hass.http.async_register_static_paths([
            StaticPathConfig(
                f"/api/{DOMAIN}/dashboard.js",
                str(assets / "dashboard.js"), cache_headers=False,
            ),
            StaticPathConfig(
                f"/api/{DOMAIN}/dashboard-legacy.js",
                str(assets / "dashboard-legacy.js"), cache_headers=False,
            ),
        ])
        register_commands(hass)
        hass.data[f"{DOMAIN}_assets_loaded"] = True
    manifest_path = Path(__file__).with_name("manifest.json")
    manifest_text = await hass.async_add_executor_job(manifest_path.read_text, "utf-8")
    frontend_version = json.loads(manifest_text)["version"]
    await panel_custom.async_register_panel(
        hass,
        frontend_url_path=PANEL_PATH,
        webcomponent_name="evohome-dashboard-panel",
        module_url=f"/api/{DOMAIN}/dashboard.js?v={frontend_version}",
        sidebar_title="Evohome",
        sidebar_icon="mdi:home-thermometer",
        require_admin=True,
        config={"domain": DOMAIN},
    )
    hass.data[f"{DOMAIN}_ui_loaded"] = True


async def async_setup_entry(hass, entry):
    tokens = StoredTokenManager(
        hass, token_cache_key(entry.data[CONF_USERNAME]), entry.data[CONF_USERNAME], entry.data[CONF_PASSWORD],
        async_get_clientsession(hass),
    )
    hub = EvohomeHub(
        hass, entry.entry_id, tokens,
        entry.options.get(CONF_SCAN_INTERVAL, DEFAULT_INTERVAL),
    )
    await hub.initialize()
    await hub.async_config_entry_first_refresh()
    # Register controller devices before zone entities need via_device_id.
    hub.system_device_ids = register_system_devices(hass, entry.entry_id, hub.client)
    hass.data.setdefault(DOMAIN, {})[entry.entry_id] = hub
    hub.rf_monitor = None
    try:
        controller_id = entry.options.get(CONF_RF_CONTROLLER_ID, "")
        if controller_id:
            # Import optional MQTT requirements only when monitoring is enabled.
            from .rf_monitor import RfDemandMonitor
            hub.rf_monitor = RfDemandMonitor(
                hass, entry.entry_id, controller_id,
                entry.options.get(CONF_RF_MQTT_TOPIC, DEFAULT_RF_MQTT_TOPIC),
                valid_zone_names(entry.options.get(CONF_RF_ZONE_NAMES, "{}")),
            )
            await hub.rf_monitor.async_start()
        await _setup_frontend(hass)
        await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    except Exception:
        if hub.rf_monitor is not None:
            await hub.rf_monitor.async_stop()
        hass.data[DOMAIN].pop(entry.entry_id, None)
        raise
    entry.async_on_unload(entry.add_update_listener(_options_updated))
    return True


async def _options_updated(hass, entry):
    await hass.config_entries.async_reload(entry.entry_id)


async def async_unload_entry(hass, entry):
    unloaded = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unloaded:
        hub = hass.data[DOMAIN].get(entry.entry_id)
        if hub is not None and hub.rf_monitor is not None:
            await hub.rf_monitor.async_stop()
        hass.data[DOMAIN].pop(entry.entry_id, None)
        if not hass.data[DOMAIN] and hass.data.get(f"{DOMAIN}_ui_loaded"):
            frontend.async_remove_panel(hass, PANEL_PATH, warn_if_unknown=False)
            hass.data[f"{DOMAIN}_ui_loaded"] = False
    return unloaded
