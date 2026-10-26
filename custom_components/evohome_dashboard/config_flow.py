"""UI configuration for a single European Resideo TCC account.

Future 3.1.0 compatibility branch; stock HA 2026.10 still pins 2.1.0.
"""
from __future__ import annotations

import logging

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.const import CONF_PASSWORD, CONF_SCAN_INTERVAL, CONF_USERNAME
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from evohomeasync2 import EvohomeClient

from .auth import StoredTokenManager, token_cache_key
from .const import DEFAULT_INTERVAL, DOMAIN, MIN_INTERVAL
from .diagnostics import classify_setup_error

_LOGGER = logging.getLogger(__name__)


class EvohomeDashboardConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    VERSION = 1

    async def async_step_user(self, user_input=None):
        errors = {}
        if user_input is not None:
            username = user_input[CONF_USERNAME].strip()
            await self.async_set_unique_id(username.lower())
            self._abort_if_unique_id_configured()
            try:
                manager = StoredTokenManager(
                    self.hass,
                    token_cache_key(username),
                    username,
                    user_input[CONF_PASSWORD],
                    async_get_clientsession(self.hass),
                )
                client = EvohomeClient(manager)
                # v3 setup loads configuration without polling locations.
                await client.setup()
                if not client.locations:
                    errors["base"] = "no_locations"
            except Exception as err:
                # Raw cloud errors may contain API response bodies or account names.
                # Never include str(err), passwords or tokens in the HA log.
                reason = classify_setup_error(err)
                _LOGGER.warning(
                    "Evohome connection setup failed: category=%s exception=%s http_status=%s",
                    reason, type(err).__name__, getattr(err, "status", None),
                )
                errors["base"] = reason
            else:
                if not errors:
                    return self.async_create_entry(
                        title="Evohome Dashboard",
                        data={**user_input, CONF_USERNAME: username},
                    )
        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema({
                vol.Required(CONF_USERNAME): str,
                vol.Required(CONF_PASSWORD): str,
            }),
            errors=errors,
        )

    @staticmethod
    def async_get_options_flow(config_entry):
        return EvohomeDashboardOptionsFlow()


class EvohomeDashboardOptionsFlow(config_entries.OptionsFlow):
    async def async_step_init(self, user_input=None):
        if user_input is not None:
            return self.async_create_entry(title="", data=user_input)
        return self.async_show_form(
            step_id="init",
            data_schema=vol.Schema({
                vol.Required(
                    CONF_SCAN_INTERVAL,
                    default=self.config_entry.options.get(CONF_SCAN_INTERVAL, DEFAULT_INTERVAL),
                ): vol.All(vol.Coerce(int), vol.Range(min=MIN_INTERVAL, max=3600)),
            }),
        )
