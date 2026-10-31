"""Prepare a stable HA release from the manifest and GitHub Releases metadata.

Firmware beta releases have tags "ramses-arduino-..." and are deliberately
ignored for HA SemVer. Never release a downgraded/duplicate integration version.

No network, side effects or GitHub permissions needed: the workflow fetches
GitHub metadata and calls this validator before creating the release.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from check_ha_version import version_from_manifest, version_tuple

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "custom_components/evohome_dashboard/manifest.json"
CHANGELOG = ROOT / "docs/HA_CHANGELOG.md"
STABLE_TAG = re.compile(r"^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")


def plan_release(version: str, releases: list[dict]) -> str:
    """Returns 'publish' or 'already_published', raises on inconsistency."""
    requested = version_tuple(version)
    if not isinstance(releases, list):
        raise ValueError("GitHub releases must be an array")
    stable = {}
    for item in releases:
        if not isinstance(item, dict):
            raise ValueError("Unexpected GitHub release item")
        tag = item.get("tag_name")
        if not isinstance(tag, str):
            raise ValueError("GitHub release lacks tag_name")
        match = STABLE_TAG.fullmatch(tag)
        if not match:
            # Ignore ESP32 tags, including their prereleases.
            continue
        if item.get("draft") or item.get("prerelease"):
            raise ValueError(f"HA SemVer tag cannot be draft/prerelease: {tag}")
        key = tuple(map(int, match.groups()))
        if key in stable:
            raise ValueError(f"Duplicate stable HA version: {tag}")
        stable[key] = item
    if stable and requested < max(stable):
        raise ValueError(
            "Refuse to publish older HA version after "
            + ".".join(map(str, max(stable)))
        )
    if requested in stable:
        return "already_published"
    return "publish"


def changelog_entry(text: str, version: str) -> str:
    """Require human-written release notes for the *exact* HA version."""
    key = f"## {version}"
    lines = text.splitlines()
    match = [i for i, line in enumerate(lines) if line.strip() == key]
    if len(match) != 1:
        raise ValueError(f"Missing or ambiguous HA changelog section: {key}")
    start = match[0] + 1
    end = next((i for i in range(start, len(lines))
                if lines[i].startswith("## ")), len(lines))
    notes = "\n".join(lines[start:end]).strip()
    if len(notes) < 20:
        raise ValueError(f"Empty HA release notes: {key}")
    return (f"# Evohome Dashboard v{version}\n\n" + notes
            + "\n\nThis is the **Home Assistant HACS integration**, not ESP32 firmware.\n"
            + "Restart Home Assistant after installing an integration update.\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--releases-json", required=True, type=Path)
    parser.add_argument("--github-output", required=True, type=Path)
    parser.add_argument("--release-notes", required=True, type=Path)
    args = parser.parse_args()
    version = ".".join(map(str, version_from_manifest(MANIFEST.read_text())))
    releases = json.loads(args.releases_json.read_text(encoding="utf-8"))
    action = plan_release(version, releases)
    # Only an unpublished stable integration version requires release notes.
    if action == "publish":
        note = changelog_entry(CHANGELOG.read_text(encoding="utf-8"), version)
        args.release_notes.write_text(note, encoding="utf-8")
    with args.github_output.open("a", encoding="utf-8") as output:
        output.write(f"version={version}\ntag=v{version}\naction={action}\n")
    print(f"HA integration v{version}: {action}; firmware releases ignored")


if __name__ == "__main__":
    main()
