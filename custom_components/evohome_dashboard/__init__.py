"""Evohome Dashboard: cloud-native Evohome integration and touchscreen panel."""
from __future__ import annotations

import logging
from pathlib import Path

from evohomeasync2 import EvohomeClient
from homeassistant.components import frontend, panel_custom, websocket_api
from homeassistant.components.http import StaticPathConfig
from homeassistant.const import CONF_PASSWORD, CONF_SCAN_INTERVAL, CONF_USERNAME
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from .auth import StoredTokenManager, token_cache_key
from .const import DEFAULT_INTERVAL, DOMAIN, PANEL_PATH
from .hub import EvohomeHub
from .websocket import register_commands

_LOGGER = logging.getLogger(__name__)
PLATFORMS = ["climate", "sensor"]


async def async_setup(hass, config):
    hass.data.setdefault(DOMAIN, {})
    return True


async def _setup_frontend(hass):
    if hass.data.get(f"{DOMAIN}_ui_loaded"):
        return
    if not hass.data.get(f"{DOMAIN}_assets_loaded"):
        resource = Path(__file__).parent / "frontend" / "dashboard.js"
        await hass.http.async_register_static_paths([
            StaticPathConfig(
                f"/api/{DOMAIN}/dashboard.js", str(resource), cache_headers=False,
            )
        ])
        register_commands(hass)
        hass.data[f"{DOMAIN}_assets_loaded"] = True
    await panel_custom.async_register_panel(
        hass,
        frontend_url_path=PANEL_PATH,
        webcomponent_name="evohome-dashboard-panel",
        module_url=f"/api/{DOMAIN}/dashboard.js",
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
    hass.data.setdefault(DOMAIN, {})[entry.entry_id] = hub
    try:
        await _setup_frontend(hass)
        await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    except Exception:
        hass.data[DOMAIN].pop(entry.entry_id, None)
        raise
    entry.async_on_unload(entry.add_update_listener(_options_updated))
    return True


async def _options_updated(hass, entry):
    await hass.config_entries.async_reload(entry.entry_id)


async def async_unload_entry(hass, entry):
    unloaded = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unloaded:
        hass.data[DOMAIN].pop(entry.entry_id, None)
        if not hass.data[DOMAIN] and hass.data.get(f"{DOMAIN}_ui_loaded"):
            frontend.async_remove_panel(hass, PANEL_PATH, warn_if_unknown=False)
            hass.data[f"{DOMAIN}_ui_loaded"] = False
    return unloaded
