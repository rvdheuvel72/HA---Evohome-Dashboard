"""Regression checks for gateway error transparency without requiring HA imports."""
from __future__ import annotations

import ast
import asyncio
from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "custom_components/evohome_dashboard/gateway_diagnostics.py"


class DummyClientError(Exception):
    pass


class FakeAiohttp:
    ClientError = DummyClientError


class FakeResponse:
    def __init__(self, status, body):
        self.status = status
        self.body = body

    async def json(self, content_type=None):
        return self.body


def load_error_handler():
    """Extract the pure response formatter; HA's import is intentionally excluded."""
    tree = ast.parse(SOURCE.read_text(encoding="utf-8"))
    handler = next(n for n in tree.body if isinstance(n, ast.AsyncFunctionDef)
                   and n.name == "_raise_gateway_http_error")
    handler.returns = None
    handler.args.args[0].annotation = None
    const = next(n for n in tree.body if isinstance(n, ast.Assign)
                 and any(isinstance(t, ast.Name) and t.id == "_GATEWAY_ERROR_HINTS"
                         for t in n.targets))
    namespace = {"aiohttp": FakeAiohttp}
    exec(compile(ast.Module(body=[const, handler], type_ignores=[]), str(SOURCE), "exec"), namespace)
    return namespace["_raise_gateway_http_error"]


class GatewayHttpErrorTests(unittest.IsolatedAsyncioTestCase):
    async def test_timeout_conflict_has_actionable_reason(self):
        handler = load_error_handler()
        with self.assertRaisesRegex(ValueError, "cooldown") as caught:
            await handler(FakeResponse(409, {"error": "one_shot_not_ready_or_in_cooldown_check_status"}), "ID25")
        self.assertIn("HTTP 409", str(caught.exception))

    async def test_old_per_request_method_is_identified(self):
        handler = load_error_handler()
        with self.assertRaisesRegex(ValueError, "global setting"):
            await handler(FakeResponse(409, {"error": "per_request_method_not_supported_update_ha_to_v0_3_8"}), "ID25")

    async def test_untrusted_body_is_not_echoed(self):
        handler = load_error_handler()
        with self.assertRaises(ValueError) as caught:
            await handler(FakeResponse(409, {"error": "password=supersecret&token=xxx"}), "ID25")
        self.assertEqual(str(caught.exception), "ID25 (HTTP 409)")

    async def test_plain_error_uses_http_status(self):
        handler = load_error_handler()
        with self.assertRaises(ValueError) as caught:
            await handler(FakeResponse(500, {"reason": "missing error"}), "Method save")
        self.assertEqual(str(caught.exception), "Method save (HTTP 500)")


if __name__ == "__main__":
    unittest.main()
