#!/usr/bin/env python3
"""Regression checks for RAMSES ESP32 HTTP delivery and background poll budget."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
# Upstream Arduino-ESP32 3.3.12 fixes WebServer header-parse stalls
# (espressif/arduino-esp32#12794); do not regress to the hanging 3.3.4.
repo_root=ROOT.parents[1]
for workflow in ('trusted-ramses-compile.yml', 'ramses-arduino-wroom32e.yml',
                 'publish-ramses-arduino-v0.4-beta.yml'):
    wf=(repo_root/'.github/workflows'/workflow).read_text()
    assert 'esp32:esp32@3.3.12' in wf, workflow
    assert 'esp32:esp32@3.3.4' not in wf, workflow
SKETCH=ROOT / "RamsesGateway"
html=(SKETCH/"GatewayWebPage.h").read_text()
script=(SKETCH/"GatewayWebScript.h").read_text()
ino=(SKETCH/"RamsesGateway.ino").read_text()
radio=(SKETCH/"Cc1101Radio.h").read_text()
startup=(SKETCH/"Cc1101Startup.h").read_text()
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
for token in ('thermostat-panel', 'pairing-panel', 'one-shot-panel',
              'rx-health-panel', 'packet-capture-panel'):
    assert token in html, token
assert "scheduled('status',poll)" in script
assert "setInterval(()=>scheduled('status',poll),5000)" in script
assert "loopback" not in script.lower()
# A blank room-temperature input must not silently turn into a 0C RF reading.
assert "if(rawTemperature==='')" in script
assert "Number(rawTemperature)" in script
assert 'web.on("/api/v1/thermostat", HTTP_GET, httpThermostatStatus);' in ino
assert 'web.on("/api/v1/opentherm/pairing", HTTP_POST, httpOtPairing);' in ino
assert "setInterval(updatePairing" not in script
assert "setInterval(updateOneShot" not in script
assert "setInterval(updatePacketCapture" not in script
assert "setInterval(updateRxHealth" not in script
assert "function refreshRxHealth()" in script
assert "document.visibilityState==='hidden'" in script
assert 'while (!radioReady)' in ino and 'RADIO_RECOVERY_RETRY_MS' in ino
assert 'SPI.transfer(0x30); // SRES' in radio
assert 'return false;' in radio
assert "version!=0x00 && version!=0xFF" in startup
assert "part==0x00" in startup
assert 'return cc1101_startup::ready(part_,version_,rxCommandAccepted);' in radio
assert 'startupRxConfirmed_=cc1101_startup::receiving(startupMarcState_);' in radio
assert 'initStage_="initialized";' in radio
assert 'startupMarcState()' in ino and 'initStage()' in ino
assert 'cc1101_init_stage' in ino
assert 'if((status(0x35)&0x1Fu)==0x0Du)return true;' not in radio
assert "GW_CC1101_FREQUENCY_KHZ" in ino
# Receiver silence may indicate missing signals or GDO2/SPI faults, never
# mandate a disruptive SRES that changes an identified chip to missing.
assert 'radioSilentRecoveryAttempts' not in ino
assert 'radioSilenceWarnings' in ino
silence_section=ino[ino.index('// Quiet RAMSES traffic'):ino.index('if (decoder.receiving() && (now - lastByteMs > RX_IDLE_TIMEOUT_MS))')]
assert 'radio.begin(' not in silence_section
assert 'no SRES issued' in silence_section
assert 'radioReady=radio.begin(' in ino  # real initialization failures still retry
# HTTP must be handled before potentially synchronous MQTT sockets.
main_loop=ino[ino.index('void loop() {'):]
assert main_loop.index('web.handleClient()')<main_loop.index('mqttConnect()')
assert 'mqttConnectLatency.observe' in main_loop
assert 'mqttLoopLatency.observe' in main_loop
assert 'httpLatency.observe' in main_loop
assert 'const uint32_t handledBefore=httpServicedCount;' in main_loop
assert 'if(httpServicedCount!=handledBefore)' in main_loop
assert 'httpUnservedSlowCalls' in main_loop
assert 'http_unserved_max_ms' in ino
assert 'http_serviced' in ino
assert 'setNoDelay(true)' in ino
for field in ('http_max_ms','mqtt_connect_max_ms','mqtt_loop_max_ms',
              'web_asset_max_ms','radio_silence_warnings'):
    assert field in ino, field
# A failed CC1101 status read returns 0xFF and must not masquerade as 0x1F.
assert 'decodeMarcState(status(0x35))' in radio
assert 'return raw==0xFFu?0xFFu:' in startup

match=re.search(r'R"GWJS\((.*)\)GWJS";',script,re.S)
assert match, "Standalone JS must be extractable from its raw header"
javascript=match.group(1)
with tempfile.TemporaryDirectory() as directory:
    code=Path(directory)/"gateway.js"
    code.write_text(javascript)
    subprocess.run(["node","--check",str(code)],check=True,timeout=15)
print("Web delivery regression: HTML asset, independent JS, rate-limited polling, bounded log batch, non-disruptive RF silence, HTTP/MQTT latency PASS")
