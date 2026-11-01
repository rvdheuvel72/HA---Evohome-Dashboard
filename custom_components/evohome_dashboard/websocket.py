"""Authenticated, admin-only websocket API for the touchscreen UI."""
from __future__ import annotations

import voluptuous as vol
from homeassistant.components import websocket_api
from .const import DOMAIN, CONF_RF_ZONE_LINKS
from .room_links import set_link


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
        ("rf_zone_link", {vol.Required("index"): str,
                          vol.Required("system_id"): str,
                          vol.Required("zone_id"): str}, "_rf_zone_link"),
        ("rf_diag_status", {}, "_rf_diag_status"),
        ("rf_diag_start", {
            vol.Required("confirm"): str,
            vol.Required("data_id"): vol.In([0, 18, 25, 117]),
            vol.Optional("replace_capture", default=False): bool,
        }, "_rf_diag_start"),
        ("rf_diag_export", {}, "_rf_diag_export"),
        ("rf_diag_existing_capture", {}, "_rf_diag_existing_capture"),
        ("zone_set", {vol.Required("system_id"): str, vol.Required("zone_id"): str,
                      vol.Required("temperature"): vol.Coerce(float),
                      vol.Optional("until"): str}, "set_zone"),
        ("zone_frost", {vol.Required("system_id"): str, vol.Required("zone_id"): str}, "set_zone_frost"),
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
                        "backend_version": hass.data.get(f"{DOMAIN}_backend_version"),
                        "r8810a_diag": (hub.rf_diag.snapshot()
                                        if getattr(hub, "rf_diag", None) is not None
                                        else {"configured": False, "running": False}),
                        "heat_demand": (hub.rf_monitor.snapshot()
                                        if getattr(hub, "rf_monitor", None) is not None
                                        else {"enabled": False,
                                              "reason": "Configure an RF controller ID in Evohome options"}),
                    }
                elif _method in ("_rf_diag_status", "_rf_diag_start",
                                    "_rf_diag_export", "_rf_diag_existing_capture"):
                    diagnostic = getattr(hub, "rf_diag", None)
                    if diagnostic is None:
                        raise ValueError(
                            "Configure local ESP32 gateway host in Evohome options first"
                        )
                    if _method == "_rf_diag_status":
                        result = diagnostic.snapshot()
                    elif _method == "_rf_diag_export":
                        result = diagnostic.export()
                    elif _method == "_rf_diag_existing_capture":
                        result = await diagnostic.existing_capture()
                    else:
                        result = await diagnostic.start(
                            confirm=msg["confirm"], data_id=msg["data_id"],
                            replace_capture=msg.get("replace_capture", False),
                        )
                elif _method == "_rf_zone_link":
                    # Select only an *existing* cloud room (stable system/zone
                    # identifiers), never guess cloud ordering from RF index.
                    entry = hass.config_entries.async_get_entry(hub.entry_id)
                    if entry is None or getattr(hub, "rf_monitor", None) is None:
                        raise ValueError("RF monitoring is not configured")
                    links = set_link(
                        entry.options.get(CONF_RF_ZONE_LINKS, {}),
                        msg["index"], msg["system_id"], msg["zone_id"],
                        hub.data,
                    )
                    hass.config_entries.async_update_entry(
                        entry, options={**entry.options, CONF_RF_ZONE_LINKS: links}
                    )
                    result = {"ok": True, "links": links}
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
