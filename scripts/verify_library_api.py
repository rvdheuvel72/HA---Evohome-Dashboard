"""Verify our backend is built for the EXACT version pinned in manifest.json.

Run on self-hosted CI with evohome-async==2.1.0 installed. No credentials needed.
"""
from __future__ import annotations

import ast
from inspect import iscoroutinefunction, signature
import json
from pathlib import Path

from evohomeasync2 import EvohomeClient, Location, Zone, ControlSystem, HotWater

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / "custom_components/evohome_dashboard/manifest.json").read_text())
assert "evohome-async==2.1.0" in manifest["requirements"]

assert callable(EvohomeClient.update)
assert iscoroutinefunction(EvohomeClient.update)
assert "dont_update_status" in signature(EvohomeClient.update).parameters
assert iscoroutinefunction(Location.update)
assert iscoroutinefunction(Zone.set_temperature)
assert iscoroutinefunction(Zone.reset)
assert iscoroutinefunction(Zone.get_schedule)
assert iscoroutinefunction(Zone.set_schedule)
assert iscoroutinefunction(ControlSystem.set_mode)
assert iscoroutinefunction(HotWater.set_state)
assert not hasattr(EvohomeClient, "setup"), "Version changed; review calls and PUT semantics"

for path in ("config_flow.py", "hub.py"):
    tree = ast.parse((root / "custom_components/evohome_dashboard" / path).read_text())
    methods = [(node.attr, node.lineno) for node in ast.walk(tree)
               if isinstance(node, ast.Attribute)]
    for forbidden in ("setup", "get_status", "wait"):
        assert not any(attr == forbidden for attr, _ in methods), (
            path, forbidden, "Method not exposed by pinned v2.1.0"
        )

print("Pinned evohome-async 2.1.0 runtime API contract passed")
