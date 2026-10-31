"""Enforce a Home Assistant integration version increase for runtime changes.

Equivalent to Hyundai Battery Insight's add-on version gate, but reads the
HACS integration's custom_components/evohome_dashboard/manifest.json.

Usage:
  python scripts/check_ha_version.py --base-sha <trusted PR base SHA>

Only *integration* changes require a version bump; firmware, independent
frontend packaging, CI, tests and documentation do not.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = "custom_components/evohome_dashboard/manifest.json"
INTEGRATION_PREFIX = "custom_components/evohome_dashboard/"
SEMVER = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)")


def version_tuple(value: object) -> tuple[int, int, int]:
    """Only stable numeric SemVer is allowed for the published HA integration."""
    if not isinstance(value, str) or not SEMVER.fullmatch(value):
        raise ValueError(f"Invalid stable HA integration version: {value!r}")
    return tuple(int(x) for x in value.split("."))


def version_from_manifest(manifest: str) -> tuple[int, int, int]:
    obj = json.loads(manifest)
    if not isinstance(obj, dict) or obj.get("domain") != "evohome_dashboard":
        raise ValueError("Unexpected integration manifest/domain")
    return version_tuple(obj.get("version"))


def require_bump(base_manifest: str, head_manifest: str, changed: list[str]) -> str:
    """Reject decreased versions and changed runtime files without a version bump."""
    old = version_from_manifest(base_manifest)
    new = version_from_manifest(head_manifest)
    runtime = sorted(path for path in changed if path.startswith(INTEGRATION_PREFIX))
    if new < old:
        raise ValueError(f"HA version must not decrease: {old} → {new}")
    if runtime and new == old:
        raise ValueError(
            "HA integration runtime files changed without manifest version bump: "
            + ", ".join(runtime[:12])
        )
    return ".".join(map(str, new))


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True)


def check(base: str, head: str) -> str:
    if not re.fullmatch(r"[0-9a-fA-F]{40}", base):
        raise ValueError("Expected complete trusted base SHA")
    baseline = git("show", f"{base}:{MANIFEST}")
    current = git("show", f"{head}:{MANIFEST}")
    changed = git("diff", "--no-renames", "--name-only", base, head).splitlines()
    return require_bump(baseline, current, changed)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-sha", required=True)
    parser.add_argument("--head", default="HEAD")
    args = parser.parse_args()
    version = check(args.base_sha, args.head)
    print(f"HA manifest SemVer validated: v{version}")


if __name__ == "__main__":
    main()
