"""Account-scoped persistent token manager. No tokens in diagnostics or logs."""
from __future__ import annotations

from typing import Any
from evohomeasync2.auth import AbstractTokenManager
from homeassistant.helpers.storage import Store


class StoredTokenManager(AbstractTokenManager):
    """Persist Resideo tokens in HA private storage; one cache per account/config entry."""

    def __init__(self, hass: Any, key: str, username: str, password: str, session: Any) -> None:
        super().__init__(username, password, session)
        self._store = Store(hass, 1, f"evohome_dashboard_tokens_{key}", private=True)
        self._loaded = False

    async def load_access_token(self) -> None:
        if self._loaded:
            return
        self._loaded = True
        cached = await self._store.async_load()
        if cached and cached.get("username") == self._client_id:
            try:
                self._import_access_token(cached)
            except (ValueError, KeyError, TypeError):
                self.clear_access_token()

    async def get_access_token(self) -> str:
        await self.load_access_token()
        return await super().get_access_token()

    async def save_access_token(self) -> None:
        await self._store.async_save({"username": self._client_id} | self._export_access_token())
