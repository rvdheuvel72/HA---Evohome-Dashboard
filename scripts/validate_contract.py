"""Dependency and packaging contract checks (no HA import required)."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
target = root / "custom_components" / "evohome_dashboard"
manifest = json.loads((target / "manifest.json").read_text())
assert manifest["domain"] == "evohome_dashboard"
assert manifest["config_flow"] is True
assert manifest["iot_class"] == "cloud_polling"
assert manifest["requirements"] == ["evohome-async==2.1.0"], "Match HA 2026.10 built-in evohome dependency"
assert "colegates" not in json.dumps(manifest).lower()
for required in ("__init__.py", "auth.py", "config_flow.py", "hub.py", "websocket.py",
                 "schedules.py", "model.py", "climate.py", "sensor.py",
                 "frontend/dashboard.js", "frontend/dashboard-legacy.js", "strings.json"):
    assert (target / required).is_file(), required
assert (root / "hacs.json").is_file()
assert (root / "docs" / "ARCHITECTURE.md").is_file()
# The backend loader is intentionally minimal. UI features are independently
# shipped in the HACS Dashboard module, with the original UI kept as fallback.
loader = (target / "frontend/dashboard.js").read_text()
assert "/hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js" in loader
js = (root / "frontend-package/dist/evohome-dashboard-ui.js").read_text()
assert (root / "frontend-package/hacs.json").is_file()
for tab in ("overview", "rooms", "demand", "schedules", "actions", "settings", "errors"):
    assert f'["{tab}"' in js
print("Evohome Phase 1 packaging contract OK")
