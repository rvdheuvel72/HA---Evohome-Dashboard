"""Optional read-only MQTT transport for the RAMSES room-demand ledger.

Uses the existing gateway /rx topic; never publishes MQTT or sends RF commands.
"""
from __future__ import annotations

import logging

from homeassistant.components import mqtt
from homeassistant.helpers.dispatcher import async_dispatcher_send
from homeassistant.helpers.storage import Store

from .heat_demand import HeatDemandLedger

_LOGGER = logging.getLogger(__name__)


class RfDemandMonitor:
    def __init__(self, hass, entry_id: str, controller_id: str,
                 topic: str, zone_names: dict[int, str]):
        self.hass = hass
        self.entry_id = entry_id
        self.topic = topic
        self.ledger = HeatDemandLedger(
            controller_id, zone_names=zone_names, time_zone=hass.config.time_zone
        )
        self._store = Store(hass, 1, f"evohome_dashboard_rf_demand_{entry_id}", private=True)
        self._unsubscribe = None
        self.connected = False
        self.signal = f"evohome_dashboard_rf_demand_{entry_id}"

    async def async_start(self):
        saved = await self._store.async_load()
        self.ledger.restore_daily(saved)
        try:
            self._unsubscribe = await mqtt.async_subscribe(
                self.hass, self.topic, self._on_message, qos=0
            )
        except Exception as err:
            # MQTT is optional: the cloud Evohome dashboard must still work.
            # Never include MQTT configuration or a broker error's credentials.
            _LOGGER.warning("RF monitoring unavailable (%s); verify the MQTT integration",
                            type(err).__name__)
            self.connected = False
            return
        self.connected = True

    def _on_message(self, message):
        accepted, newly_seen_zone = self.ledger.observe_mqtt(message.payload)
        if not accepted:
            return
        # Store delay batches flash/disk writes, Home Assistant history still
        # records individual observed zone demand changes as sensor states.
        self._store.async_delay_save(self.ledger.daily_storage, 300)
        async_dispatcher_send(self.hass, self.signal, newly_seen_zone)

    def snapshot(self):
        result = self.ledger.snapshot()
        result["mqtt_connected"] = self.connected
        result["topic"] = self.topic
        return result

    async def async_stop(self):
        if self._unsubscribe is not None:
            self._unsubscribe()
            self._unsubscribe = None
        self.connected = False
        await self._store.async_save(self.ledger.daily_storage())
