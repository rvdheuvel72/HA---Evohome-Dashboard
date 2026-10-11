"""Conservative verification for Evohome time-limited heating overrides.

A reported expiry is valid only if it equals the request or the controller's
documented precision-reducing representations (seconds/minutes truncated).
A time merely nearby, such as a later minute, is NOT evidence of success.
"""
from __future__ import annotations

from datetime import datetime, timezone

_UNSET = object()


def _utc(value: datetime) -> datetime | None:
    if value.tzinfo is None or value.utcoffset() is None:
        return None
    return value.astimezone(timezone.utc)


def matches_override_expiry(
    actual: datetime | None,
    requested: datetime | None,
    *,
    previous: datetime | None | object = _UNSET,
) -> bool:
    """Verify request, rejecting stale state when only duration was changed.

    Resideo may return an ISO timestamp without fractions or seconds. The
    accepted values are exactly the requested UTC time, floor to second, or
    floor to minute. A previous observed expiry is never proof of a changed
    expiry unless the request itself is identical to that value.
    """
    if requested is None:
        return actual is None
    if actual is None:
        return False
    expected = _utc(requested)
    observed = _utc(actual)
    if expected is None or observed is None:
        return False
    candidates = {
        expected,
        expected.replace(microsecond=0),
        expected.replace(second=0, microsecond=0),
    }
    if observed not in candidates:
        return False
    if previous is not _UNSET and previous is not None:
        old = _utc(previous)
        if old is not None and old == observed and old != expected:
            return False
    return True
