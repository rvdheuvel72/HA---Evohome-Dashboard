"""Offline tests for the HA-side R8810A diagnostic. No network or radio."""
from __future__ import annotations

import asyncio
import importlib.util
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "custom_components/evohome_dashboard/r8810a_diagnostic.py"
spec = importlib.util.spec_from_file_location("r8810a_diagnostic_pure", SCRIPT)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

TSV = """# RAMSES capture for unit test
sequence\tlocal_time\tutc_epoch_ms\tuptime_ms\ttype\trssi_magnitude\tmessage
1\ttime\t1\t10100\tpacket\t80\t096 RQ --- 01:245914 10:048994 --:------ 3220 005 0000120000
2\ttime\t2\t10200\tpacket\t60\t094 RP --- 10:048994 01:245914 --:------ 3220 005 00C01201CD
3\ttime\t3\t10500\tpacket\t60\t094 RP --- 10:048994 01:245914 --:------ 3220 005 00C0193800
4\ttime\t4\t10900\tpacket\t60\t094 RP --- 10:048994 01:245914 --:------ 3220 005 0070750000
5\ttime\t5\t10950\tpacket\t60\t094 RP --- 10:048994 01:245914 --:------ 3220 005 00F0750000
6\ttime\t6\t10750\tpacket\t60\t094 RP --- 10:999999 01:111111 --:------ 3220 005 00C0193800
"""


class FakeGateway:
    def __init__(self, *, count=0, fail_at=None, bad_stop=False):
        self.existing_count = count
        self.fail_at = fail_at
        self.bad_stop = bad_stop
        self.calls = []
        self.started = False
        self.current_id = 18
        self.uptime = 10000

    async def get(self, path):
        self.calls.append(("get", path))
        if path == mod.CAPTURE + "/status":
            return {"active": self.started, "count": self.existing_count,
                    "full": False}
        if path == mod.STATUS:
            self.uptime += 300
            return {"uptime_ms": self.uptime}
        if path == mod.READ:
            return {"ready": True, "data_id": self.current_id,
                    "controller_id": 245914, "bridge_id": 48994,
                    "state": "timeout" if self.current_id == 117 else "read_ack",
                    "has_value": self.current_id != 117,
                    "value": 1.75 if self.current_id == 18 else 55.25,
                    "raw": 450, "tx_phase": "local_complete",
                    "tx_bytes": 44, "tx_expected_bytes": 44}
        raise AssertionError(path)

    async def post(self, path, fields, config):
        self.calls.append(("post", path, fields.copy(), config))
        if path == mod.CAPTURE + "/action":
            self.started = fields["action"] == "start"
            if fields["action"] == "stop" and self.bad_stop:
                raise ValueError("simulated broken stop response")
            return {"ok": True}
        if path == mod.READ:
            self.current_id = int(fields["data_id"])
            if self.current_id == self.fail_at:
                raise mod.DiagnosticError("Simulated unavailable gateway read")
            return {"queued": True, "read_only": True}
        raise AssertionError(path)

    async def capture(self):
        self.calls.append(("get", "capture_download"))
        return TSV


class RFDiagnosticTests(unittest.IsolatedAsyncioTestCase):
    def make(self, **kwargs):
        fake = FakeGateway(**kwargs)
        service = mod.R8810aDiagnostic(fake, sleep=lambda _: asyncio.sleep(0))
        return fake, service

    def test_host_restricts_ssrf(self):
        for host in ("evohome-gateway", "evohome-gateway.local",
                     "192.168.1.12", "10.1.2.3", "172.16.1.20"):
            with self.subTest(host=host):
                self.assertEqual(mod.valid_gateway_host(host), host)
        for host in ("http://evohome-gateway", "google.com", "127.0.0.1",
                     "169.254.169.254", "192.168.1.5:8123",
                     "192.168.1.5/foo", "0.0.0.0", "8.8.8.8", "::1"):
            with self.subTest(host=host):
                with self.assertRaises(mod.DiagnosticError):
                    mod.valid_gateway_host(host)

    async def test_no_auth_means_no_radio_or_capture_changes(self):
        fake, service = self.make()
        with self.assertRaisesRegex(mod.DiagnosticError, "authorization"):
            await service.start(confirm="yes", data_id=25)
        self.assertEqual(fake.calls, [])
        with self.assertRaisesRegex(mod.DiagnosticError, "Only IDs"):
            await service.start(confirm=mod.CONFIRM, data_id=255)
        self.assertEqual(fake.calls, [])

    async def test_one_shot_id25_does_not_read_others(self):
        fake, service = self.make()
        await service.start(confirm=mod.CONFIRM, data_id=25)
        await service._task
        self.assertEqual([x["id"] for x in service.reads], [25])
        self.assertEqual(service.state, "complete")
        self.assertEqual(service.reads[0]["value"], 55.25)
        commands = [x for x in fake.calls if x[0] == "post" and x[1] == mod.READ]
        self.assertEqual(len(commands), 1)
        self.assertEqual(commands[0][2]["data_id"], "25")
        self.assertEqual(commands[0][3], "paired-bridge-read")
        self.assertEqual(commands[0][2]["confirm"],
                         "one-read-own-r8810a-as-observed-controller")

    async def test_all_three_sequential_and_downloadable(self):
        fake, service = self.make()
        await service.start(confirm=mod.CONFIRM, data_id=0)
        await service._task
        report = service.export()
        self.assertEqual([x["id"] for x in service.reads], [18, 25, 117])
        self.assertEqual(service.reads[-1]["state"], "timeout")
        self.assertTrue(report["capture_tsv"].startswith("# RAMSES"))
        self.assertEqual([x[2]["action"] for x in fake.calls
                          if x[0] == "post" and x[1] == mod.CAPTURE + "/action"],
                         ["start", "stop"])
        self.assertEqual(service.snapshot()["packet_count"], 6)
        self.assertIn("inconclusive", report["report"]["interpretation"])

    async def test_refuses_to_replace_existing_capture_without_opt_in(self):
        fake, service = self.make(count=3)
        with self.assertRaisesRegex(mod.DiagnosticError, "Export it first"):
            await service.start(confirm=mod.CONFIRM, data_id=25)
        self.assertFalse(any(x[0] == "post" for x in fake.calls))
        saved = await service.existing_capture()
        self.assertEqual(saved["status"]["count"], 3)
        self.assertIn("3220", saved["capture_tsv"])
        await service.start(confirm=mod.CONFIRM, data_id=25, replace_capture=True)
        await service._task
        self.assertEqual(service.state, "complete")

    async def test_gateway_failure_returns_partial_report(self):
        fake, service = self.make(fail_at=25)
        await service.start(confirm=mod.CONFIRM, data_id=0)
        await service._task
        self.assertEqual(service.state, "interrupted")
        self.assertEqual([x["id"] for x in service.reads], [18])
        self.assertIn("Simulated", service.error)
        self.assertTrue(service.export()["capture_tsv"])

    async def test_invalid_stop_json_does_not_discard_capture(self):
        fake, service = self.make(bad_stop=True)
        await service.start(confirm=mod.CONFIRM, data_id=25)
        await service._task
        self.assertEqual(service.state, "interrupted")
        self.assertIn("stop: ValueError", service.capture_error)
        self.assertTrue(service.export()["capture_tsv"])

    async def test_parallel_run_is_blocked(self):
        fake, service = self.make()
        await service.start(confirm=mod.CONFIRM, data_id=25)
        with self.assertRaisesRegex(mod.DiagnosticError, "already running"):
            await service.start(confirm=mod.CONFIRM, data_id=25)
        await service._task

    def test_opentherm_parity_and_foreign_rf_addresses(self):
        frames = mod.decode_capture(TSV)
        self.assertEqual(len(frames), 6)
        self.assertFalse(frames[4]["ot_valid"])
        self.assertIsNone(frames[4]["ot_reply"])
        read = [{"id": 25, "controller_id": 245914, "bridge_id": 48994,
                 "start_uptime_ms": 10200, "end_uptime_ms": 10600}]
        mod.annotate(read, frames)
        self.assertEqual(len(read[0]["rf_reply_candidates"]), 1)
        self.assertEqual(read[0]["rf_reply_candidates"][0]["source"], "10:048994")

    async def test_gateway_http_capture_reads_every_streamed_chunk(self):
        # ESP32 emits a TSV header and one sendContent() call per event.
        # A one-shot read() can return only the first chunk.
        from types import SimpleNamespace

        class Content:
            def __init__(self, chunks):
                self.chunks = chunks
            async def iter_chunked(self, size):
                for chunk in self.chunks:
                    yield chunk

        class Response:
            status = 200
            def __init__(self, chunks):
                self.content = Content(chunks)
            async def __aenter__(self):
                return self
            async def __aexit__(self, *args):
                return False

        class Session:
            def __init__(self, chunks):
                self.chunks = chunks
                self.kwargs = None
            def request(self, method, url, **kwargs):
                self.kwargs = kwargs
                return Response(self.chunks)

        data = TSV.encode("utf-8")
        client = object.__new__(mod.GatewayHTTP)
        client.host = "evohome-gateway"
        client.auth = None
        class DummyClientError(Exception):
            pass
        client._aiohttp = SimpleNamespace(
            ClientTimeout=lambda total: total,
            ClientError=DummyClientError,
        )
        client.session = Session([data[:10], data[10:52], data[52:]])
        actual = await client.capture() if hasattr(client, "capture") else None
        self.assertEqual(actual, TSV)
        self.assertFalse(client.session.kwargs["allow_redirects"])
        self.assertEqual(mod.decode_capture(actual), mod.decode_capture(TSV))

        client.session = Session([b"a" * 70000, b"b" * 70000, b"c"])
        with self.assertRaisesRegex(mod.DiagnosticError, "size limit"):
            await client.capture()

    async def test_missing_gateway_config_has_no_secret_in_ui_status(self):
        _, service = self.make()
        snapshot = service.snapshot()
        self.assertNotIn("password", str(snapshot).lower())
        self.assertEqual(snapshot["state"], "idle")


if __name__ == "__main__":
    unittest.main()
