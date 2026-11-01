"""UI configuration for a single European Resideo TCC account.

Use the HA 2026.10 native evohome-async 2.1.0 API; avoid dependency conflicts.
"""
from __future__ import annotations

import logging

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.const import CONF_PASSWORD, CONF_SCAN_INTERVAL, CONF_USERNAME
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from evohomeasync2 import EvohomeClient

from .auth import StoredTokenManager, token_cache_key
from .const import (
    DEFAULT_INTERVAL, DOMAIN, MIN_INTERVAL,
    CONF_RF_CONTROLLER_ID, CONF_RF_MQTT_TOPIC, CONF_RF_ZONE_NAMES,
    DEFAULT_RF_MQTT_TOPIC,
)
from .heat_demand import valid_controller_id, valid_zone_names
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
                # HA 2026.10 shares evohomeasync2 with its native integration.
                # v2.1.0 loads configuration without polling locations here.
                await client.update(dont_update_status=True)
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
        errors = {}
        if user_input is not None:
            controller = user_input.get(CONF_RF_CONTROLLER_ID, "").strip()
            topic = user_input.get(CONF_RF_MQTT_TOPIC, DEFAULT_RF_MQTT_TOPIC).strip()
            names = user_input.get(CONF_RF_ZONE_NAMES, "{}")
            if controller and not valid_controller_id(controller):
                errors[CONF_RF_CONTROLLER_ID] = "invalid_controller_id"
            if (not topic or len(topic) > 128 or not topic.endswith("/rx")
                    or any(c.isspace() for c in topic)):
                errors[CONF_RF_MQTT_TOPIC] = "invalid_mqtt_topic"
            try:
                valid_zone_names(names)
            except (ValueError, TypeError):
                errors[CONF_RF_ZONE_NAMES] = "invalid_zone_mapping"
            if not errors:
                return self.async_create_entry(
                    title="", data={**self.config_entry.options, **user_input,
                                    CONF_RF_CONTROLLER_ID: controller,
                                    CONF_RF_MQTT_TOPIC: topic}
                )
        return self.async_show_form(
            step_id="init",
            errors=errors,
            data_schema=vol.Schema({
                vol.Required(
                    CONF_SCAN_INTERVAL,
                    default=self.config_entry.options.get(CONF_SCAN_INTERVAL, DEFAULT_INTERVAL),
                ): vol.All(vol.Coerce(int), vol.Range(min=MIN_INTERVAL, max=3600)),
                vol.Optional(
                    CONF_RF_CONTROLLER_ID,
                    default=self.config_entry.options.get(CONF_RF_CONTROLLER_ID, ""),
                ): str,
                vol.Optional(
                    CONF_RF_MQTT_TOPIC,
                    default=self.config_entry.options.get(
                        CONF_RF_MQTT_TOPIC, DEFAULT_RF_MQTT_TOPIC),
                ): str,
                vol.Optional(
                    CONF_RF_ZONE_NAMES,
                    default=self.config_entry.options.get(CONF_RF_ZONE_NAMES, "{}"),
                ): str,
            }),
        )
