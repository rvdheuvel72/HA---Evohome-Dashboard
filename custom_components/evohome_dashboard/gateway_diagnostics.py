"""Restricted, on-demand read-only OpenTherm ID25 gateway diagnostics."""
from __future__ import annotations

import asyncio
from ipaddress import ip_address

import aiohttp
from homeassistant.helpers.aiohttp_client import async_get_clientsession

PATH = "/api/v1/opentherm/one-shot"
METHOD_PATH = "/api/v1/opentherm/tx-method"


# Gateway error codes are more useful than a bare HTTP status. They contain
# no credentials; never forward the request URL, headers or Basic Auth data.
_GATEWAY_ERROR_HINTS = {
    "per_request_method_not_supported_update_ha_to_v0_3_8":
        "The gateway rejected a per-test transport choice; update/reload the HA dashboard and use the global setting.",
    "one_shot_not_ready_or_in_cooldown_check_status":
        "The ID25 read is not ready or remains in its cooldown; read status before retrying.",
    "rf_tx_disabled_or_not_configured":
        "RF read-request transmission is disabled or the gateway radio is not configured.",
    "previous_changes_waiting_for_wifi_confirmation":
        "Another gateway configuration change is awaiting Wi-Fi confirmation.",
}


async def _raise_gateway_http_error(response: aiohttp.ClientResponse, operation: str) -> None:
    """Surface the gateway's bounded error code; preserve HTTP status if absent."""
    reason = ""
    try:
        data = await response.json(content_type=None)
        if isinstance(data, dict) and isinstance(data.get("error"), str):
            code = data["error"]
            # Do not echo arbitrary remote text (or credential-like material).
            if 0 < len(code) <= 100 and all(c.isalnum() or c == "_" for c in code):
                reason = _GATEWAY_ERROR_HINTS.get(code, code)
    except (ValueError, aiohttp.ClientError):
        pass
    detail = f": {reason}" if reason else ""
    raise ValueError(f"{operation} (HTTP {response.status}){detail}")


def validate_gateway_ip(host: str) -> str:
    """Accept only private IPv4 LAN destinations, never arbitrary URLs or DNS."""
    try:
        address = ip_address(host)
    except ValueError as exc:
        raise ValueError("Enter the gateway LAN IPv4 address (not a URL)") from exc
    if address.version != 4 or not address.is_private or address.is_loopback or address.is_link_local or address.is_multicast or address.is_unspecified or address.is_reserved:
        raise ValueError("Gateway must have a private LAN IPv4 address")
    return str(address)


async def gateway_id25(hass, host: str, username: str, password: str, action: str, method: str = "fifo") -> dict:
    """Proxy only the firmware's ID25 status/trigger endpoint. No credentials stored."""
    gateway = validate_gateway_ip(host)
    if action not in ("status", "trigger", "method_get", "method_set") or method not in ("uart", "fifo"):
        raise ValueError("Unsupported diagnostic operation")
    if not username or not password:
        raise ValueError("Enter gateway Basic Auth credentials")
    if action in ("trigger", "method_set") and len(password) < 12:
        raise ValueError("Gateway requires a strong password for this operation")
    session = async_get_clientsession(hass)
    kwargs = {
        "auth": aiohttp.BasicAuth(username, password),
        "timeout": aiohttp.ClientTimeout(total=10),
        "allow_redirects": False,
    }
    try:
        if action == "method_get":
            async with session.get(f"http://{gateway}{METHOD_PATH}", **kwargs) as response:
                if response.status == 404:
                    raise ValueError("Shared method requires ESP32 gateway firmware beta.38 or later")
                if response.status != 200:
                    await _raise_gateway_http_error(response, "Gateway method read failed")
                return await response.json(content_type=None)
        if action == "method_set":
            async with session.post(
                f"http://{gateway}{METHOD_PATH}",
                data={"method": method},
                headers={"X-Gateway-Config": "tx-method"},
                **kwargs,
            ) as response:
                if response.status != 200:
                    await _raise_gateway_http_error(response, "Gateway method save failed")
                return await response.json(content_type=None)
        if action == "status":
            async with session.get(f"http://{gateway}{PATH}", **kwargs) as response:
                if response.status != 200:
                    await _raise_gateway_http_error(response, "Gateway status read failed")
                return await response.json(content_type=None)
        async with session.post(
            f"http://{gateway}{PATH}",
            data={"confirm": "experimental-single-read-25"},
            headers={"X-Gateway-Config": "one-shot"},
            **kwargs,
        ) as response:
            if response.status != 202:
                await _raise_gateway_http_error(response, "Gateway rejected ID25 test")
            return await response.json(content_type=None)
    except (aiohttp.ClientError, asyncio.TimeoutError) as exc:
        raise ValueError("Gateway unreachable or request timed out") from exc
