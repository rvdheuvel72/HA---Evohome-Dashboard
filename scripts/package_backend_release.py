"""Build a deterministic, secret-free Home Assistant/HACS backend release ZIP.

Run from repository root:
    python3 scripts/package_backend_release.py --expected-version 0.3.2
The ZIP contains custom_components/evohome_dashboard/, not ESP32 firmware.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "custom_components" / "evohome_dashboard"
ALLOWED_SUFFIXES = {".py", ".json", ".js"}
REQUIRED = {
    "__init__.py", "manifest.json", "config_flow.py", "hub.py",
    "heat_demand.py", "rf_monitor.py", "sensor.py", "websocket.py",
    "frontend/dashboard.js", "frontend/dashboard-legacy.js",
}


def package(expected_version: str, output_dir: Path | None = None) -> tuple[Path, Path]:
    manifest = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))
    version = manifest.get("version")
    if version != expected_version:
        raise ValueError(f"Manifest version {version!r} != release {expected_version!r}")
    if manifest.get("domain") != "evohome_dashboard":
        raise ValueError("Unexpected Home Assistant domain")
    files = sorted(f for f in SOURCE.rglob("*") if f.is_file())
    names = {f.relative_to(SOURCE).as_posix() for f in files}
    if not REQUIRED <= names:
        raise ValueError(f"Release integration files missing: {sorted(REQUIRED - names)}")
    if not files or any(
        f.is_symlink()
        or f.suffix not in ALLOWED_SUFFIXES
        or any(part.startswith(".") or part == "__pycache__" for part in f.relative_to(SOURCE).parts)
        or "secret" in f.name.lower()
        or f.stat().st_size > 2_000_000
        for f in files
    ):
        raise ValueError("Unexpected or potentially sensitive release file")

    output = (output_dir or ROOT / "dist")
    output.mkdir(parents=True, exist_ok=True)
    filename = f"Evohome-Dashboard-v{version}-HACS.zip"
    archive = output / filename
    with ZipFile(archive, "w", compression=ZIP_DEFLATED, compresslevel=9) as z:
        for file in files:
            name = f"custom_components/evohome_dashboard/{file.relative_to(SOURCE).as_posix()}"
            info = ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, file.read_bytes())
    with ZipFile(archive) as z:
        if z.testzip() is not None:
            raise ValueError("Corrupt archive")
        if len(z.namelist()) != len(files):
            raise ValueError("Unexpected ZIP contents")
        packaged = json.loads(z.read("custom_components/evohome_dashboard/manifest.json"))
        if packaged["version"] != expected_version:
            raise ValueError("ZIP contains an incorrect manifest version")

    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    checksum = output / f"{filename}.sha256"
    checksum.write_text(f"{digest}  {filename}\n", encoding="ascii")
    return archive, checksum


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--expected-version", required=True)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    bundle, checksum = package(args.expected_version, args.output_dir)
    print(f"Backend HACS release: {bundle} ({bundle.stat().st_size} bytes)")
    print(f"SHA256: {checksum.read_text().strip()}")


if __name__ == "__main__":
    main()
