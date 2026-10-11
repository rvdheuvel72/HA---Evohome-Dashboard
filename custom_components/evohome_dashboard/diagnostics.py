"""Secret-free exception categorization shared with the configuration flow.

The caller intentionally never emits third-party exception text: vendor error
messages may include API response content. The exception class and HTTP status
are sufficient to diagnose authentication, throttling and transport failures.
"""
from __future__ import annotations

from asyncio import TimeoutError


def classify_setup_error(error: Exception) -> str:
    """Classify errors raised by evohome-async 2.1.0 without leaking secrets."""
    names = {cls.__name__ for cls in type(error).__mro__}
    status = getattr(error, "status", None)

    if status == 429 or "ApiRateLimitExceededError" in names:
        return "rate_limited"
    if "BadUserCredentialsError" in names or "AuthenticationFailedError" in names:
        return "invalid_auth"
    if ("BadApiResponseError" in names or "InvalidConfigError" in names
            or "InvalidStatusError" in names):
        return "invalid_response"
    if (isinstance(error, (ConnectionError, TimeoutError))
            or "ClientError" in names or "ClientConnectionError" in names
            or "ClientConnectorError" in names or "TimeoutError" in names):
        return "cannot_connect"
    if status is not None:
        return "cannot_connect" if status in (502, 503, 504) else "unknown"
    return "unknown"
