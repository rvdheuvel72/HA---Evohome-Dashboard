"""Restricted, on-demand read-only OpenTherm ID25 gateway diagnostics."""
from __future__ import annotations

import asyncio
from ipaddress import ip_address

import aiohttp
from homeassistant.helpers.aiohttp_client import async_get_clientsession

PATH = "/api/v1/opentherm/one-shot"


def validate_gateway_ip(host: str) -> str:
    """Accept only private IPv4 LAN destinations, never arbitrary URLs or DNS."""
    try:
        address = ip_address(host)
    except ValueError as exc:
        raise ValueError("Enter the gateway LAN IPv4 address (not a URL)") from exc
    if address.version != 4 or not address.is_private or address.is_loopback or address.is_link_local or address.is_multicast or address.is_unspecified or address.is_reserved:
        raise ValueError("Gateway must have a private LAN IPv4 address")
    return str(address)


async def gateway_id25(hass, host: str, username: str, password: str, action: str, method: str = "uart") -> dict:
    """Proxy only the firmware's ID25 status/trigger endpoint. No credentials stored."""
    gateway = validate_gateway_ip(host)
    if action not in ("status", "trigger") or method not in ("uart", "fifo"):
        raise ValueError("Unsupported diagnostic operation")
    if not username or not password:
        raise ValueError("Enter gateway Basic Auth credentials")
    if action == "trigger" and len(password) < 12:
        raise ValueError("Gateway requires a strong password for this operation")
    session = async_get_clientsession(hass)
    kwargs = {
        "auth": aiohttp.BasicAuth(username, password),
        "timeout": aiohttp.ClientTimeout(total=10),
        "allow_redirects": False,
    }
    try:
        if action == "status":
            async with session.get(f"http://{gateway}{PATH}", **kwargs) as response:
                if response.status != 200:
                    raise ValueError(f"Gateway returned HTTP {response.status}")
                return await response.json(content_type=None)
        async with session.post(
            f"http://{gateway}{PATH}",
            data={"confirm": "experimental-single-read-25", "method": method},
            headers={"X-Gateway-Config": "one-shot"},
            **kwargs,
        ) as response:
            if response.status != 202:
                raise ValueError(f"Gateway rejected ID25 test (HTTP {response.status})")
            return await response.json(content_type=None)
    except (aiohttp.ClientError, asyncio.TimeoutError) as exc:
        raise ValueError("Gateway unreachable or request timed out") from exc
