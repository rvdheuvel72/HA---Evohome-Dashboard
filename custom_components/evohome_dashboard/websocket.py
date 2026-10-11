"""Authenticated, admin-only websocket API for the touchscreen UI."""
from __future__ import annotations

import voluptuous as vol
from homeassistant.components import websocket_api
from .const import DOMAIN


def _hub(hass, entry_id):
    try:
        return hass.data[DOMAIN][entry_id]
    except KeyError as err:
        raise ValueError("Evohome connection unavailable") from err


def register_commands(hass):
    """All operations are restricted to Home Assistant administrators."""
    for name, fields, method in (
        ("entries", {}, "_entries"),
        ("status", {}, "_status"),
        ("errors", {}, "_errors"),
        ("refresh", {}, "_refresh"),
        ("zone_set", {vol.Required("system_id"): str, vol.Required("zone_id"): str,
                      vol.Required("temperature"): vol.Coerce(float),
                      vol.Optional("until"): str}, "set_zone"),
        ("zone_resume", {vol.Required("system_id"): str,
                         vol.Required("zone_id"): str}, "resume_zone"),
        ("system_mode", {vol.Required("system_id"): str, vol.Required("mode"): str,
                         vol.Optional("until"): str}, "set_system_mode"),
        ("hotwater_set", {vol.Required("system_id"): str, vol.Required("state"): str,
                          vol.Optional("until"): str}, "set_hotwater"),
        ("hotwater_resume", {vol.Required("system_id"): str}, "resume_hotwater"),
        ("schedule_get", {vol.Required("system_id"): str,
                          vol.Required("zone_id"): str}, "get_zone_schedule"),
        ("schedule_set", {vol.Required("system_id"): str, vol.Required("zone_id"): str,
                          vol.Required("schedule"): list,
                          vol.Required("expected_version"): str}, "save_zone_schedule"),
    ):
        schema = {vol.Required("type"): f"{DOMAIN}/{name}"} | ({} if name == "entries" else {vol.Required("entry_id"): str}) | fields

        @websocket_api.websocket_command(schema)
        @websocket_api.async_response
        async def handler(hass, connection, msg, _method=method):
            if not connection.user.is_admin:
                connection.send_error(msg["id"], "unauthorized", "Administrator access required")
                return
            try:
                if _method == "_entries":
                    connection.send_result(msg["id"], {"entries": [{"entry_id": key} for key in hass.data.get(DOMAIN, {})]})
                    return
                hub = _hub(hass, msg["entry_id"])
                if _method == "_status":
                    result = {
                        "state": hub.data if hub.data else {"locations": []},
                        "online": hub.last_update_success, "errors": len(hub.error_data()),
                        "entry_id": hub.entry_id,
                    }
                elif _method == "_errors":
                    result = {"events": hub.error_data()}
                elif _method == "_refresh":
                    await hub.async_request_refresh()
                    result = {"ok": hub.last_update_success}
                elif _method == "schedule_set":
                    result = await hub.save_zone_schedule(
                        msg["system_id"], msg["zone_id"], msg["schedule"], msg["expected_version"]
                    )
                else:
                    args = {key: value for key, value in msg.items()
                            if key not in ("id", "type", "entry_id")}
                    result = await getattr(hub, _method)(**args)
                connection.send_result(msg["id"], result)
            except ValueError as err:
                connection.send_error(msg["id"], "invalid_request", str(err)[:180])
            except Exception:
                connection.send_error(
                    msg["id"], "evohome_failed",
                    "Resideo operation failed; consult Evohome Dashboard error log",
                )

        websocket_api.async_register_command(hass, handler)
