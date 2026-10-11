"""Runtime smoke and serialized write verification against installed Home Assistant."""
from __future__ import annotations

import asyncio
import unittest
from datetime import datetime, timezone, timedelta
from types import SimpleNamespace

from custom_components.evohome_dashboard.hub import EvohomeHub
from custom_components.evohome_dashboard import PLATFORMS


class FakeTask:
    async def wait(self):
        return None


class FakeCoordinator:
    def __init__(self):
        self._write_lock = asyncio.Lock()
        self.previous = []
        self.completed = 0

    async def _wait_for_state(self, location, predicate):
        assert predicate(), "State predicate failed"
        self.completed += 1

    async def async_request_refresh(self):
        pass

    async def _record(self, operation, exception, target):
        raise AssertionError("Unexpected error recorded")


class RuntimeTests(unittest.IsolatedAsyncioTestCase):
    async def test_serialized_prior_expiry_snapshot(self):
        hub = FakeCoordinator()
        t0 = datetime(2026, 10, 11, 10, tzinfo=timezone.utc)
        holder = {"until": t0}
        first = t0 + timedelta(hours=1)
        second = t0 + timedelta(hours=2)

        async def issue(new_expiry):
            async def execute():
                await asyncio.sleep(0.015)
                holder["until"] = new_expiry
                return FakeTask()

            def check(previous):
                hub.previous.append(previous)
                return holder["until"] == new_expiry

            return await EvohomeHub._execute(
                hub, "test", "zone", execute, object(), check,
                previous_expiry=lambda: holder["until"],
            )

        a,b = await asyncio.gather(issue(first), issue(second))
        self.assertTrue(a["verified"] and b["verified"])
        self.assertEqual(hub.previous, [t0, first])

    async def test_all_native_platforms_declared(self):
        self.assertEqual(set(PLATFORMS),
                         {"climate","sensor","binary_sensor","select","switch","button"})


if __name__ == "__main__":
    unittest.main()
