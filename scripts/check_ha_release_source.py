"""Allow publishing a reviewed HA commit after unrelated main-branch pushes.

GitHub's workflow_run completion can occur after another commit lands on main.
Comparing SHAs alone strands a valid release when the later commit only affects
firmware or unrelated documentation. Compare the release-critical tree paths;
fail closed if any relevant file changed. GitHub's compare API is separately
checked to prove the validated SHA is an ancestor of current main.
"""
from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFIXES = ("custom_components/evohome_dashboard/",)
FILES = frozenset({
    "hacs.json",
    "docs/HA_CHANGELOG.md",
    "scripts/check_ha_version.py",
    "scripts/check_ha_release_readiness.py",
    "scripts/check_ha_release_source.py",
    "scripts/check_release_readiness.py",
    "scripts/check_codex_review.py",
    "scripts/plan_ha_release.py",
    ".github/workflows/publish-evohome-hacs.yml",
    ".github/workflows/phase1-self-hosted.yml",
})


def release_critical(path: str) -> bool:
    return path in FILES or any(path.startswith(prefix) for prefix in PREFIXES)


def changed_critical(paths: list[str]) -> list[str]:
    return sorted(path for path in paths if release_critical(path))


def check(verified_sha: str, current_sha: str) -> list[str]:
    for value in (verified_sha, current_sha):
        if not re.fullmatch(r"[0-9a-fA-F]{40}", value):
            raise ValueError("Expected exact Git SHA")
    changed = subprocess.check_output(
        ["git", "diff", "--no-renames", "--name-only", verified_sha, current_sha],
        cwd=ROOT, text=True
    ).splitlines()
    blockers = changed_critical(changed)
    if blockers:
        raise ValueError(
            "Main advanced with release-critical integration changes; wait for "
            "the newer main validation: " + ", ".join(blockers[:12])
        )
    return changed


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--verified", required=True)
    p.add_argument("--main", required=True)
    args = p.parse_args()
    unrelated = check(args.verified, args.main)
    print(f"HA release tree verified unchanged; {len(unrelated)} unrelated "
          "firmware/documentation paths are allowed.")


if __name__ == "__main__":
    main()
