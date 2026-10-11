"""Fail on breaking changes to pinned evohome-async v3.0.0 API."""
import ast
import json
from inspect import iscoroutinefunction, signature
from pathlib import Path
from evohomeasync2 import EvohomeClient, Location, Zone, ControlSystem, HotWater, CommTask

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / "custom_components/evohome_dashboard/manifest.json").read_text())
assert manifest["requirements"] == ["evohome-async==3.0.0"], "Pin/update adapter and tests together"
assert iscoroutinefunction(EvohomeClient.setup)
assert iscoroutinefunction(Location.get_status)
assert iscoroutinefunction(CommTask.wait)
for function in (Zone.set_temperature, Zone.reset, Zone.get_schedule, Zone.set_schedule,
                 ControlSystem.set_mode, HotWater.set_state, HotWater.reset):
    assert iscoroutinefunction(function), function
for file in ("config_flow.py", "hub.py"):
    s = (root / "custom_components/evohome_dashboard" / file).read_text()
    ast.parse(s)
    assert "dont_update_status=True" not in s
    assert "loc.update()" not in s
print("Pinned evohome-async 3.0.0 runtime contract OK")
