#!/usr/bin/env python3
"""Regression checks for RAMSES ESP32 HTTP delivery and background poll budget."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SKETCH=ROOT / "RamsesGateway"
html=(SKETCH/"GatewayWebPage.h").read_text()
script=(SKETCH/"GatewayWebScript.h").read_text()
ino=(SKETCH/"RamsesGateway.ino").read_text()
radio=(SKETCH/"Cc1101Radio.h").read_text()
assert 'static const char GW_DEBUG_PAGE[] PROGMEM' in html
assert 'static const char GW_WEB_JS[] PROGMEM' in script
assert html.count('<script defer src="/gateway.js"></script>')==1
assert '<script>' not in html, "No inline JS should remain in HTML"
assert html.count('</script>')==1, "Only the one external JS closing tag is allowed"
assert len(html)<21000, "HTML response must be less than half old monolithic 41KB page"
assert len(script)<26000
assert 'web.on("/gateway.js", HTTP_GET, httpGatewayJs);' in ino
assert 'web.sendContent_P(data+pos,n)' in ino
assert 'constexpr size_t BLOCK=1024' in ino
assert 'web.setContentLength(length)' in ino
assert 'MAX_SCAN=20' in ino and 'web.send(200,"application/json",json)' in ino
assert 'web.sendContent(item)' not in ino
for token in ('tx-loopback-panel', 'pairing-panel', 'one-shot-panel',
              'rx-health-panel', 'packet-capture-panel'):
    assert token in html, token
assert "scheduled('status',poll)" in script
assert "setInterval(()=>scheduled('status',poll),5000)" in script
assert "setInterval(updateLoopback" not in script
assert "setInterval(updatePairing" not in script
assert "setInterval(updateOneShot" not in script
assert "setInterval(updatePacketCapture" not in script
assert "setInterval(updateRxHealth" not in script
assert "function refreshRxHealth()" in script
assert "document.visibilityState==='hidden'" in script
assert 'while (!radioReady)' in ino and 'RADIO_RECOVERY_RETRY_MS' in ino
assert 'SPI.transfer(0x30); // SRES' in radio
assert 'return false;' in radio
assert "version_!=0x00 && version_!=0xFF" in radio
assert "status(0x35)&0x1Fu" in radio
assert "GW_CC1101_FREQUENCY_KHZ" in ino

match=re.search(r'R"GWJS\((.*)\)GWJS";',script,re.S)
assert match, "Standalone JS must be extractable from its raw header"
javascript=match.group(1)
with tempfile.TemporaryDirectory() as directory:
    code=Path(directory)/"gateway.js"
    code.write_text(javascript)
    subprocess.run(["node","--check",str(code)],check=True,timeout=15)
print("Web delivery regression: HTML asset, independent JS, rate-limited polling, bounded log batch, warm CC1101 retry PASS")
