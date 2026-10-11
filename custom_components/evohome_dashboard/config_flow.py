"""UI configuration for a single Resideo account."""
from __future__ import annotations

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.const import CONF_PASSWORD, CONF_SCAN_INTERVAL, CONF_USERNAME
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from evohomeasync2 import EvohomeClient
from .auth import StoredTokenManager
from .const import DEFAULT_INTERVAL, DOMAIN, MIN_INTERVAL


class EvohomeDashboardConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    VERSION = 1

    async def async_step_user(self, user_input=None):
        errors = {}
        if user_input:
            await self.async_set_unique_id(user_input[CONF_USERNAME].strip().lower())
            self._abort_if_unique_id_configured()
            try:
                manager = StoredTokenManager(
                    self.hass, "validation", user_input[CONF_USERNAME],
                    user_input[CONF_PASSWORD], async_get_clientsession(self.hass),
                )
                client = EvohomeClient(manager)
                await client.setup()
            except Exception as err:  # API errors vary between versions
                from evohomeasync2 import exceptions
                errors["base"] = (
                    "invalid_auth" if isinstance(err, exceptions.AuthenticationFailedError)
                    else "cannot_connect"
                )
            else:
                return self.async_create_entry(
                    title=f"Evohome ({user_input[CONF_USERNAME]})", data=user_input
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
