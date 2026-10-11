"""Compare requested Evohome override expirations with reported cloud state.

Never claim the new override was applied based only on a matching temperature
or mode: the preceding temporary override may have a different end time.
The cloud can remove sub-minute precision, so allow <= 59 seconds. Deliberately
DO NOT treat a different ten-minute slot as a successful write.
"""
from __future__ import annotations

from datetime import datetime, timezone

MAX_EXPIRY_SKEW_SECONDS = 59


def matches_override_expiry(actual: datetime | None, requested: datetime | None) -> bool:
    """Strict comparison of optional TZ-aware end times, allowing seconds truncation."""
    if requested is None:
        return actual is None
    if actual is None:
        return False
    if requested.tzinfo is None or requested.utcoffset() is None:
        return False
    if actual.tzinfo is None or actual.utcoffset() is None:
        return False
    delta = abs((actual.astimezone(timezone.utc) - requested.astimezone(timezone.utc)).total_seconds())
    return delta <= MAX_EXPIRY_SKEW_SECONDS
