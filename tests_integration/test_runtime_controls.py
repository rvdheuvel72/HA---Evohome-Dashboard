"""Runtime smoke and serialized write verification against installed Home Assistant."""
from __future__ import annotations

import asyncio
import unittest
from datetime import datetime, timezone, timedelta
from types import SimpleNamespace

from custom_components.evohome_dashboard.hub import EvohomeHub
from custom_components.evohome_dashboard import PLATFORMS


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
                async def wait():
                    await asyncio.sleep(0)
                return SimpleNamespace(wait=wait)

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

    async def test_v3_setup_and_commtask_api_is_installed(self):
        from inspect import iscoroutinefunction
        from importlib.metadata import version
        from _evohome.exceptions import EvohomeError
        from evohomeasync2 import EvohomeClient, Location, Zone, CommTask
        self.assertEqual(version("evohome-async"), "3.1.0")
        self.assertTrue(issubclass(EvohomeError, Exception))
        self.assertTrue(iscoroutinefunction(EvohomeClient.setup))
        self.assertTrue(iscoroutinefunction(Location.get_status))
        self.assertTrue(iscoroutinefunction(Zone.set_temperature))
        self.assertTrue(iscoroutinefunction(CommTask.wait))

    async def test_commtask_waits_before_cloud_readback(self):
        hub = FakeCoordinator()
        events = []
        async def write():
            events.append("put")
            async def wait():
                events.append("task_completed")
            return SimpleNamespace(wait=wait)
        async def verify(location, predicate):
            events.append("get_status")
            self.assertTrue(predicate())
        async def refresh():
            events.append("refresh")
        hub._wait_for_state = verify
        hub.async_request_refresh = refresh
        result = await EvohomeHub._execute(hub, "zone", "id", write, object(), lambda: True)
        self.assertTrue(result["verified"])
        self.assertEqual(events, ["put", "task_completed", "get_status", "refresh"])

    async def test_commtask_failure_fails_closed(self):
        hub = FakeCoordinator()
        events = []
        async def write():
            async def wait():
                raise RuntimeError("comm task failed")
            return SimpleNamespace(wait=wait)
        async def record(operation, exc, target):
            events.append("recorded")
        async def verify(location, predicate):
            events.append("incorrect_verification")
        hub._record = record
        hub._wait_for_state = verify
        with self.assertRaisesRegex(RuntimeError, "comm task failed"):
            await EvohomeHub._execute(hub, "zone", "id", write, object(), lambda: True)
        self.assertEqual(events, ["recorded"])

    async def test_schedule_waits_for_commtask_before_get(self):
        import inspect
        code = inspect.getsource(EvohomeHub.save_zone_schedule)
        self.assertIn("task = await zone.set_schedule(validated)", code)
        self.assertIn("await task.wait()", code)
        self.assertLess(code.index("await task.wait()"),
                        code.index("confirmed = normalize(await zone.get_schedule())"))

    async def test_all_native_platforms_declared(self):
        self.assertEqual(set(PLATFORMS),
                         {"climate","sensor","binary_sensor","select","switch","button"})


if __name__ == "__main__":
    unittest.main()
