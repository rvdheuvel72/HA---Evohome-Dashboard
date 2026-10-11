"""Check safe classifications without Home Assistant, credentials or cloud calls."""
from __future__ import annotations
import importlib.util
import pathlib
import unittest

path = pathlib.Path(__file__).resolve().parents[1] / "custom_components/evohome_dashboard/diagnostics.py"
spec = importlib.util.spec_from_file_location("evohome_diagnostics", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
classify = module.classify_setup_error

class AuthenticationFailedError(Exception):
    def __init__(self, status=None):
        self.status = status

class BadUserCredentialsError(AuthenticationFailedError):
    pass

class ApiRateLimitExceededError(Exception):
    pass

class InvalidConfigError(Exception):
    pass

class ClientConnectorError(Exception):
    pass

class ApiCallFailedError(Exception):
    def __init__(self, status=None):
        self.status = status

class ApiCompatibilityTests(unittest.TestCase):
    def test_bad_password(self):
        self.assertEqual(classify(BadUserCredentialsError(400)), "invalid_auth")
    def test_login_rate_limit_from_auth(self):
        self.assertEqual(classify(AuthenticationFailedError(429)), "rate_limited")
    def test_api_rate_limit(self):
        self.assertEqual(classify(ApiRateLimitExceededError()), "rate_limited")
    def test_cloud_bad_config(self):
        self.assertEqual(classify(InvalidConfigError()), "invalid_response")
    def test_network(self):
        self.assertEqual(classify(ClientConnectorError()), "cannot_connect")
    def test_gateway_outage(self):
        self.assertEqual(classify(ApiCallFailedError(503)), "cannot_connect")
    def test_integration_programming_fault(self):
        self.assertEqual(classify(AttributeError("Missing method")), "unknown")
    def test_raw_messages_not_classified_as_auth(self):
        self.assertEqual(classify(ValueError("password and secret")), "unknown")
