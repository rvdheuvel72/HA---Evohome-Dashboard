"""Exact v3.1.0 API contract for the experimental upgrade branch."""
import ast
import json
from importlib.metadata import version
from inspect import iscoroutinefunction, signature
from pathlib import Path

from evohomeasync2 import CommTask, EvohomeClient, Location, Zone, ControlSystem, HotWater

root = Path(__file__).resolve().parents[1]
folder = root / "custom_components/evohome_dashboard"
manifest = json.loads((folder / "manifest.json").read_text())
assert manifest["requirements"] == ["evohome-async==3.1.0"]
assert version("evohome-async") == "3.1.0"
assert iscoroutinefunction(EvohomeClient.setup)
assert iscoroutinefunction(Location.get_status)
assert iscoroutinefunction(CommTask.wait)
assert "interval" in signature(CommTask.wait).parameters
for method in (Zone.set_temperature, Zone.reset, Zone.get_schedule, Zone.set_schedule,
               ControlSystem.set_mode, HotWater.set_state, HotWater.reset):
    assert iscoroutinefunction(method), method
flow = (folder / "config_flow.py").read_text()
hub = (folder / "hub.py").read_text()
for source in (flow, hub):
    ast.parse(source)
    assert "update(dont_update_status=True)" not in source
assert "await client.setup()" in flow
assert "await self.client.setup()" in hub
assert "loc.get_status()" in hub
assert "location.get_status()" in hub
assert hub.count("await task.wait()") == 2, "Both writes and schedules must confirm task completion"
assert "task = await zone.set_schedule(validated)" in hub
print("evohome-async v3.1.0 compatibility contract OK")
