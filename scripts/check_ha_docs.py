"""Prevent user-visible HA integration changes from silently leaving docs stale.

This checks changed file presence and a matching documented version. It cannot
prove prose correctness; review must verify actual claims against source.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMP = "custom_components/evohome_dashboard/"
MANIFEST = COMP + "manifest.json"


def require_docs(changed: list[str], version: str, readme: str, changelog: str, guide: str) -> None:
    runtime = any(p.startswith(COMP) and p != MANIFEST for p in changed)
    manifest_changed = MANIFEST in changed
    if not runtime and not manifest_changed:
        return
    if "README.md" not in changed:
        raise ValueError("Integration change requires a matching README.md update in the same PR")
    if "docs/HA_CHANGELOG.md" not in changed:
        raise ValueError("Integration change requires docs/HA_CHANGELOG.md update")
    if not re.search(r"(?m)^## " + re.escape(version) + r"\s*$", changelog):
        raise ValueError(f"Missing exact integration changelog version: {version}")
    if f"v{version}" not in readme:
        raise ValueError(f"README does not mention current manifest version v{version}")
    ui = any(p.startswith(COMP + "frontend/") or p == COMP + "websocket.py"
             for p in changed)
    if ui and "docs/USER_GUIDE.md" not in changed:
        raise ValueError("Dashboard UI/API changes require docs/USER_GUIDE.md update")
    if ui and f"v{version}" not in guide:
        raise ValueError(f"User guide does not mention current integration v{version}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-sha", required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[a-fA-F0-9]{40}", args.base_sha):
        raise ValueError("Expected complete base SHA")
    changed = subprocess.check_output(
        ["git", "diff", "--no-renames", "--name-only", args.base_sha, "HEAD"],
        cwd=ROOT, text=True,
    ).splitlines()
    version = json.loads((ROOT / MANIFEST).read_text(encoding="utf-8"))["version"]
    require_docs(changed, version,
                 (ROOT / "README.md").read_text(encoding="utf-8"),
                 (ROOT / "docs/HA_CHANGELOG.md").read_text(encoding="utf-8"),
                 (ROOT / "docs/USER_GUIDE.md").read_text(encoding="utf-8"))
    print(f"Documentation change/version gate passed for HA v{version}")


if __name__ == "__main__":
    main()
