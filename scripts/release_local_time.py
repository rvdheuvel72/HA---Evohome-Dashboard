#!/usr/bin/env python3
"""Add the real GitHub publication date in Netherlands local time to releases.

Always use the publishedAt timestamp from GitHub, not workflow run time.
Formatting is Europe/Amsterdam (CET in winter / CEST in summer).
Safe to rerun: exactly one release-date header, body and notes preserved.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from zoneinfo import ZoneInfo

LOCAL_ZONE = ZoneInfo("Europe/Amsterdam")
MARKER = "**Release date (CET/CEST, Europe/Amsterdam): "
HEADER_RE = re.compile(
    r"^\*\*Release date \(CET/CEST, Europe/Amsterdam\): [^\r\n]*\*\*\r?\n(?:\r?\n)?"
)


def release_header(published_at: str) -> str:
    """Convert authoritative GitHub ISO timestamp to local CET or CEST."""
    instant = datetime.fromisoformat(published_at.replace("Z", "+00:00"))
    if instant.tzinfo is None:
        raise ValueError("GitHub publishedAt must have a timezone offset")
    local = instant.astimezone(LOCAL_ZONE)
    offset = local.strftime("%z")
    offset = f"{offset[:3]}:{offset[3:]}"
    return (
        f"{MARKER}{local:%Y-%m-%d %H:%M:%S} "
        f"{local:%Z} (UTC{offset})**"
    )


def annotate(body: str, published_at: str) -> str:
    header = release_header(published_at)
    body = body or ""
    # Only remove our own marker, not authored content or other dates.
    while HEADER_RE.match(body):
        body = HEADER_RE.sub("", body, count=1)
    return f"{header}\n\n{body.lstrip(chr(10))}".rstrip() + "\n"


def gh_json(*args: str):
    p = subprocess.run(
        ["gh", "release", *args],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(p.stdout)


def update(tag: str, dry_run: bool) -> bool:
    data = gh_json("view", tag, "--json", "tagName,isDraft,publishedAt,body")
    if data.get("isDraft"):
        print(f"Skip draft: {tag}")
        return False
    published_at = data.get("publishedAt")
    if not published_at:
        print(f"Skip release without publishedAt: {tag}")
        return False
    old = data.get("body") or ""
    new = annotate(old, published_at)
    if new == old:
        print(f"Already dated: {tag}")
        return False
    print(f"{'Would date' if dry_run else 'Date'} release {tag}: "
          f"{release_header(published_at)}")
    if not dry_run:
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "notes.md"
            path.write_text(new, encoding="utf-8")
            subprocess.run(
                ["gh", "release", "edit", tag, "--notes-file", str(path)],
                check=True,
            )
        # Do not report success unless the public description has been updated.
        check = gh_json("view", tag, "--json", "body")
        if check.get("body", "").strip() != new.strip():
            raise RuntimeError(f"Published release notes verification failed: {tag}")
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--all", action="store_true", help="Backfill all releases")
    group.add_argument("--tag", help="Stamp one new published release")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if args.tag:
        update(args.tag, args.dry_run)
    else:
        releases = gh_json("list", "--limit", "1000",
                           "--json", "tagName,isDraft")
        for item in releases:
            if not item.get("isDraft"):
                update(item["tagName"], args.dry_run)


if __name__ == "__main__":
    main()
