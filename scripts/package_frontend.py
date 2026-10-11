"""Package the independent HACS Dashboard UI for publication to its own repo.

Usage:
    python3 scripts/package_frontend.py --output build/evohome-dashboard-ui.zip
The archive contains exactly the frontend repo root: hacs.json, README.md,
LICENSE and dist/evohome-dashboard-ui.js. Never include credentials, .storage,
the Python integration or the legacy fallback in this independent package.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED, ZipInfo

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "frontend-package"
FILES = (
    "hacs.json",
    "README.md",
    "LICENSE",
    "dist/evohome-dashboard-ui.js",
)


def validate_package() -> None:
    manifest = json.loads((SOURCE / "hacs.json").read_text(encoding="utf-8"))
    assert manifest["filename"] == "evohome-dashboard-ui.js"
    assert manifest["name"] == "Evohome Dashboard UI"
    for name in FILES:
        path = SOURCE / name
        assert path.is_file(), f"Missing frontend source: {name}"
    javascript = (SOURCE / FILES[-1]).read_text(encoding="utf-8")
    assert 'customElements.define("evohome-dashboard-panel"' in javascript
    assert 'customElements.define("evohome-dashboard-card"' in javascript
    assert "evohome_dashboard/" in javascript
    assert "evohomeasync2" not in javascript
    assert "CONF_PASSWORD" not in javascript


def build(output: Path) -> Path:
    validate_package()
    output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(output, "w", ZIP_DEFLATED) as archive:
        for name in FILES:
            entry = ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            entry.compress_type = ZIP_DEFLATED
            entry.external_attr = 0o644 << 16
            archive.writestr(entry, (SOURCE / name).read_bytes())
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path,
                        default=ROOT / "build" / "evohome-dashboard-ui.zip")
    args = parser.parse_args()
    print("Standalone HACS Dashboard package:", build(args.output))
