"""Optional read-only MQTT transport for the RAMSES room-demand ledger.

Uses the existing gateway /rx topic; never publishes MQTT or sends RF commands.
"""
from __future__ import annotations

import logging
from datetime import timedelta

from homeassistant.components import mqtt
from homeassistant.core import callback
from homeassistant.helpers.dispatcher import async_dispatcher_send
from homeassistant.helpers.event import async_track_time_interval
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
        self._stop_tick = None
        self._checkpoint_pending = False
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
        # Refresh expiry/estimated minutes even when no RF frame arrives.
        self._stop_tick = async_track_time_interval(
            self.hass, self._on_tick, timedelta(minutes=1)
        )

    @callback
    def _on_message(self, message):
        accepted, newly_seen_zone = self.ledger.observe_mqtt(message.payload)
        if not accepted:
            return
        # Store delay batches flash/disk writes, Home Assistant history still
        # records individual observed zone demand changes as sensor states.
        self._queue_checkpoint()
        async_dispatcher_send(self.hass, self.signal, newly_seen_zone)

    @callback
    def _on_tick(self, _):
        self.ledger.snapshot()
        if self.ledger.zones:
            self._queue_checkpoint()
        async_dispatcher_send(self.hass, self.signal, None)

    @callback
    def _queue_checkpoint(self):
        # HA Store is debounce-based: scheduling on *every* packet can defer
        # persistence indefinitely. Keep the first 5-minute deadline fixed.
        if self._checkpoint_pending:
            return
        self._checkpoint_pending = True
        self._store.async_delay_save(self._checkpoint_data, 300)

    @callback
    def _checkpoint_data(self):
        self._checkpoint_pending = False
        return self.ledger.daily_storage()

    def snapshot(self):
        result = self.ledger.snapshot()
        result["mqtt_connected"] = self.connected
        result["topic"] = self.topic
        return result

    async def async_stop(self):
        if self._stop_tick is not None:
            self._stop_tick()
            self._stop_tick = None
        if self._unsubscribe is not None:
            self._unsubscribe()
            self._unsubscribe = None
        self.connected = False
        await self._store.async_save(self.ledger.daily_storage())
