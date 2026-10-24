#!/usr/bin/env python3
"""RAMSES local WSL-Ubuntu lab. Only the manual main-branch workflow may flash."""
from __future__ import annotations

import base64
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import time
from urllib.parse import urlparse
from urllib.request import Request, build_opener, HTTPRedirectHandler, urlopen

VERSION = "1.5.1"
CLI_URL = ("https://github.com/arduino/arduino-cli/releases/download/v1.5.1/"
           "arduino-cli_1.5.1_Linux_64bit.tar.gz")
CLI_SHA256 = "28a8e119c498a25607821c36cb2dc49e8463941b261a0d99091baa7bc692dd2b"
CORE = "esp32:esp32@3.3.12"
FQBN = "esp32:esp32:esp32"
OUT = Path("local-lab-output")
SKETCH = Path("firmware/arduino-wroom32e/RamsesGateway")
CLI = Path.home() / ".local/share/ramses-arduino-cli" / VERSION / "arduino-cli"
DATA = Path.home() / ".cache/ramses-arduino-lab"
PREFS = DATA / "arduino-cli.yaml"


def command(argv, **kwargs):
    subprocess.run([str(x) for x in argv], check=True, **kwargs)


def save(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def cli_setup():
    CLI.parent.mkdir(parents=True, exist_ok=True)
    if CLI.exists():
        probe = subprocess.run([str(CLI), "version"], capture_output=True, text=True)
        if probe.returncode == 0 and VERSION in probe.stdout:
            return CLI
    with tempfile.TemporaryDirectory(prefix="ramses-cli-") as temp:
        archive = Path(temp) / "cli.tar.gz"
        with urlopen(CLI_URL, timeout=90) as response, archive.open("wb") as dest:
            shutil.copyfileobj(response, dest)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if digest != CLI_SHA256:
            raise RuntimeError("Arduino CLI v1.5.1 upstream SHA256 mismatch")
        with tarfile.open(archive, "r:gz") as tar:
            member = tar.getmember("arduino-cli")
            if not member.isfile() or member.issym() or member.islnk():
                raise RuntimeError("Unexpected Arduino CLI archive member")
            source = tar.extractfile(member)
            if source is None:
                raise RuntimeError("Arduino CLI binary missing")
            candidate = CLI.with_suffix(".new")
            with source, candidate.open("wb") as dst:
                shutil.copyfileobj(source, dst)
            candidate.chmod(0o755)
            candidate.replace(CLI)
    command([CLI, "version"])
    return CLI


def env_setup():
    DATA.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.update({
        "ARDUINO_CONFIG_FILE": str(PREFS),
        "ARDUINO_DIRECTORIES_DATA": str(DATA / "data"),
        "ARDUINO_DIRECTORIES_USER": str(DATA / "sketchbook"),
        "ARDUINO_DIRECTORIES_DOWNLOADS": str(DATA / "downloads"),
    })
    for p in ("data", "sketchbook", "downloads"):
        (DATA / p).mkdir(parents=True, exist_ok=True)
    if not PREFS.exists():
        command([CLI, "config", "init", "--dest-file", PREFS, "--overwrite"], env=env)
    command([CLI, "config", "add", "board_manager.additional_urls",
             "https://espressif.github.io/arduino-esp32/package_esp32_index.json"], env=env)
    command([CLI, "core", "update-index"], env=env)
    command([CLI, "core", "install", CORE], env=env)
    command([CLI, "lib", "install", "PubSubClient@2.8"], env=env)
    return env


def attached_serial():
    # A Windows COM3 name is not a Linux serial device. Never infer COM3 -> ttyUSB0.
    base = Path("/dev/serial/by-id")
    return sorted(str(p) for p in base.iterdir() if p.is_symlink()) if base.is_dir() else []


def checked_serial(expected):
    base = Path("/dev/serial/by-id")
    candidate = Path(expected)
    if not expected or candidate.parent != base or not candidate.is_symlink():
        raise RuntimeError("WSL USB unavailable or no approved /dev/serial/by-id symlink")
    actual = candidate.resolve(strict=True)
    if not re.fullmatch(r"tty(?:USB|ACM)\d+", actual.name) or not stat.S_ISCHR(actual.stat().st_mode):
        raise RuntimeError("Approved USB serial identity does not resolve to a serial character device")
    return candidate, actual


def probe():
    codex = shutil.which("codex")
    save("inventory.json", {
        "platform": sys.platform,
        "wsl": "microsoft" in Path("/proc/sys/kernel/osrelease").read_text().lower(),
        "arduino_cli": str(CLI),
        "arduino_cli_version": VERSION,
        "codex_available": bool(codex),
        "usb_serial_by_id": attached_serial(),
        "note": "COM3 is Windows-only until its USB device is attached to WSL via usbipd-win.",
    })


def gateway_base():
    base = os.getenv("LAB_GATEWAY_URL", "http://evohome-gateway.local").strip().rstrip("/")
    parsed = urlparse(base)
    if parsed.scheme != "http" or parsed.username or parsed.password or parsed.path or parsed.query or parsed.fragment:
        raise RuntimeError("Gateway API must use a trusted LAN HTTP origin only")
    host = parsed.hostname or ""
    try:
        allowed = ipaddress.ip_address(host).is_private
    except ValueError:
        allowed = host.endswith(".local")
    if not allowed:
        raise RuntimeError("Gateway origin must use a private IP or .local hostname")
    return base


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        raise RuntimeError("Gateway API redirect rejected")


def sample_api():
    user = os.getenv("LAB_GATEWAY_USER", "")
    password = os.getenv("LAB_GATEWAY_PASSWORD", "")
    if not user or not password:
        raise RuntimeError("Missing gateway HTTP credentials (runner secrets)")
    auth = base64.b64encode((user + ":" + password).encode()).decode("ascii")
    base = gateway_base()
    opener = build_opener(NoRedirect)
    result = []
    for i in range(3):
        captured = {}
        for name, endpoint in (
            ("status", "/api/v1/status"),
            ("rx_health", "/api/v1/radio/rx-health"),
            ("radio_diagnostics", "/api/v1/radio/diagnostics"),
            ("uart_probe", "/api/v1/radio/uart-probe"),
        ):
            req = Request(base + endpoint, headers={"Authorization": "Basic " + auth})
            try:
                with opener.open(req, timeout=12) as response:
                    body = json.load(response)
            except Exception as ex:
                if name == "uart_probe":
                    # beta.20 has no UART probe endpoint yet. Do not block
                    # diagnostics or recovery flashing on older firmware.
                    captured[name] = {"available": False, "reason": type(ex).__name__}
                    continue
                raise RuntimeError("Gateway API request failed: " + name + " (" +
                                   type(ex).__name__ + ")") from None
            if name == "status":
                # Whitelist: never write credentials, hostname, IP, WiFi or MQTT secrets.
                keys = ("version", "mode", "radio_detected", "radio_verified",
                        "cc1101_part", "cc1101_version", "cc1101_boot_marcstate",
                        "cc1101_rx_at_boot", "cc1101_frequency_khz", "rf_uart_bytes",
                        "rf_syncs", "valid_frames", "rejected_frames", "dropped_frames",
                        "radio_recalibrations", "radio_recalibration_failures",
                        "radio_silence_warnings", "http_last_ms", "http_max_ms",
                        "http_slow_calls", "mqtt_connect_max_ms", "mqtt_loop_max_ms",
                        "uptime_ms", "free_heap")
                captured[name] = {k: body.get(k) for k in keys}
            elif name in ("rx_health", "uart_probe"):
                captured[name] = body
            else:
                captured[name] = {"rejected": body.get("rejected"),
                                  "last_rejection": body.get("last_rejection")}
        captured["collected_utc_ms"] = int(time.time() * 1000)
        result.append(captured)
        if i < 2:
            time.sleep(15)
    save("gateway-rx-health.json", result)
    a, b = result[0]["status"], result[-1]["status"]
    save("rx-summary.json", {
        "uart_delta": b["rf_uart_bytes"] - a["rf_uart_bytes"],
        "sync_delta": b["rf_syncs"] - a["rf_syncs"],
        "valid_delta": b["valid_frames"] - a["valid_frames"],
        "last_marcstate": result[-1]["rx_health"].get("latest", {}).get("marcstate"),
        "last_rx_active": result[-1]["rx_health"].get("latest", {}).get("in_rx"),
    })


def codex_analyze():
    if os.getenv("LAB_USE_CODEX", "false").lower() != "true":
        return
    if not shutil.which("codex"):
        raise RuntimeError("Codex was requested but unavailable in WSL Ubuntu runner account")
    command(["codex", "exec", "--sandbox", "read-only", "--output-last-message",
             str(OUT / "codex-analysis.txt"),
             ("Review local-lab-output/gateway-rx-health.json including UART probe hex and the firmware CC1101 "
              "RX implementation. Do not edit files, transmit RF, pair, or flash. "
              "Clearly distinguish verified observations from hypotheses, and "
              "recommend the smallest safe change.")], timeout=240)


def build_and_maybe_flash(flash):
    if flash:
        if os.getenv("LAB_CONFIRM_FLASH") != "FLASH-WSL-USB":
            raise RuntimeError("Manual flash confirmation FLASH-WSL-USB required")
        requested = os.getenv("LAB_SERIAL_BY_ID", "")
        _, before = checked_serial(requested)
    else:
        requested, before = "", None
    env = env_setup()
    with tempfile.TemporaryDirectory(prefix="ramses-sketch-") as work:
        sketch = Path(work) / "RamsesGateway"
        shutil.copytree(SKETCH, sketch, ignore=shutil.ignore_patterns("GatewaySecrets.h"))
        header_env = os.getenv("LAB_SECRETS_HEADER_PATH", "")
        private_header = (Path(header_env).expanduser() if header_env else
                          Path.home() / ".config/ramses-lab/GatewaySecrets.h")
        if flash and not private_header.is_file():
            raise RuntimeError("Private GatewaySecrets.h missing in WSL; flash refused")
        if private_header.is_file():
            shutil.copyfile(private_header, sketch / "GatewaySecrets.h")
            (sketch / "GatewaySecrets.h").chmod(0o600)
        output = Path(work) / "build"
        command([CLI, "compile", "--fqbn", FQBN, "--warnings", "all",
                 "--output-dir", output, sketch], env=env, timeout=1100)
        save("build-result.json", {"passed": True, "fqbn": FQBN,
                                   "cli_version": VERSION, "core": CORE})
        if not flash:
            return
        # Fail closed if the USB identity or Linux device node changed during compilation.
        port, after = checked_serial(requested)
        if after != before:
            raise RuntimeError("USB port changed during compilation; flash refused")
        command([CLI, "upload", "-p", port, "--fqbn", FQBN,
                 "--input-dir", output, sketch], env=env, timeout=180)
        save("upload-result.json", {"uploaded": True, "port": str(port),
                                    "rf_reception_verified": False})
        # Record post-flash recovery separately. Upload success alone does not
        # demonstrate HTTP readiness or successful on-air RAMSES reception.
        time.sleep(10)
        try:
            sample_api()
        except Exception as ex:
            save("postflash-diagnostics.json", {
                "verified": False, "error_type": type(ex).__name__,
                "note": "USB upload succeeded, but gateway HTTP/RF is unverified"
            })
            raise RuntimeError("USB flashed; postflash gateway API was not verified") from None
        else:
            save("postflash-diagnostics.json", {
                "verified": True,
                "note": "Gateway API responded; RF reception only if valid-frame delta is positive"
            })


def main():
    OUT.mkdir(exist_ok=True)
    mode = os.getenv("LAB_OPERATION", "probe")
    if mode not in ("probe", "diagnose", "compile", "flash"):
        raise RuntimeError("Invalid lab operation")
    if os.getenv("LAB_BRANCH") != "refs/heads/main" or os.getenv("LAB_EVENT") != "workflow_dispatch":
        raise RuntimeError("Physical WSL lab is manual-only on main")
    cli_setup()
    probe()
    if mode == "diagnose":
        sample_api()
        codex_analyze()
    elif mode == "flash":
        # Recovery flashing must remain possible when the old firmware has a
        # broken HTTP server, Wi-Fi settings, or unavailable RX diagnostics.
        try:
            sample_api()
        except Exception as ex:
            save("preflash-diagnostics.json", {
                "available": False, "reason": type(ex).__name__,
                "detail": "Preflash gateway API unavailable; USB flash may proceed"
            })
        else:
            # Codex analysis is optional for recovery and may not prevent upload.
            if os.getenv("LAB_USE_CODEX", "false").lower() == "true":
                try:
                    codex_analyze()
                except Exception as ex:
                    save("preflash-codex.json", {
                        "available": False, "reason": type(ex).__name__,
                        "detail": "Codex unavailable; firmware USB flash not blocked"
                    })
    if mode in ("compile", "flash"):
        build_and_maybe_flash(mode == "flash")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print("WSL RAMSES lab FAILED: " + str(err), file=sys.stderr)
        sys.exit(1)
