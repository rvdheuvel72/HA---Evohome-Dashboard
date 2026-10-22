"""Fail on breaking changes to pinned evohome-async v2.1.0 API."""
import ast
import json
from inspect import iscoroutinefunction, signature
from pathlib import Path
from evohomeasync2 import EvohomeClient, Location, Zone, ControlSystem, HotWater

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / "custom_components/evohome_dashboard/manifest.json").read_text())
assert manifest["requirements"] == ["evohome-async==2.1.0"], "Pin/update adapter and tests together"
assert iscoroutinefunction(EvohomeClient.update)
assert iscoroutinefunction(Location.update)
for function in (Zone.set_temperature, Zone.reset, Zone.get_schedule, Zone.set_schedule,
                 ControlSystem.set_mode, HotWater.set_state, HotWater.reset):
    assert iscoroutinefunction(function), function
for file in ("config_flow.py", "hub.py"):
    s = (root / "custom_components/evohome_dashboard" / file).read_text()
    ast.parse(s)
    assert "client.setup()" not in s
    assert "loc.get_status()" not in s
assert "await client.update(dont_update_status=True)" in (root / "custom_components/evohome_dashboard/config_flow.py").read_text()
assert "await self.client.update(dont_update_status=True)" in (root / "custom_components/evohome_dashboard/hub.py").read_text()
assert "await zone.set_schedule(validated)" in (root / "custom_components/evohome_dashboard/hub.py").read_text()
assert "await task.wait()" not in (root / "custom_components/evohome_dashboard/hub.py").read_text()
print("Pinned evohome-async 3.0.0 runtime contract OK")
