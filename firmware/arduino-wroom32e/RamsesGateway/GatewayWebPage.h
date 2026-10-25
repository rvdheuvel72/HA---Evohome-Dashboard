#pragma once
#include <pgmspace.h>
// No third-party CDN or browser storage; served entirely by the ESP32.
static const char GW_DEBUG_PAGE[] PROGMEM = R"GWHTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>RAMSES Gateway — Diagnostics</title>
<style>
:root{color-scheme:dark;font-family:system-ui,Arial,sans-serif;background:#101821;color:#e8f0f7}
body{max-width:1100px;margin:auto;padding:18px}h1{font-size:1.6rem;margin:.4em 0}
small,.muted{color:#9eb0bf}header{display:flex;align-items:center;justify-content:space-between;gap:12px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(215px,1fr));gap:10px;margin:18px 0}
.card,.panel{border:1px solid #334354;border-radius:10px;background:#1b2836;padding:16px}
.card b{display:block;font-size:1.45rem;margin-top:6px;overflow-wrap:anywhere}
label{display:inline-flex;gap:5px;align-items:center}
.fields{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px;margin:15px 0}
.field{display:flex;flex-direction:column;gap:5px}
.field input,.field select{width:100%;box-sizing:border-box}
input{background:#25384b;color:#edf4ff;border:1px solid #536c85;border-radius:6px;padding:8px}
fieldset{border:1px solid #456177;border-radius:8px;margin:12px 0}
legend{padding:0 8px}
.setting-check{display:inline-flex;align-items:center;gap:7px;padding:5px}
select,button{background:#25384b;color:#edf4ff;border:1px solid #536c85;border-radius:6px;padding:7px;cursor:pointer}
pre{font-size:12px;white-space:pre-wrap;overflow-wrap:anywhere;max-height:420px;overflow:auto;line-height:1.45}
.controls{display:flex;align-items:center;flex-wrap:wrap;gap:10px;margin-bottom:8px}
.ok{color:#7be2a2}.bad{color:#ff9f9b}
a{color:#a9d5ff}
[hidden]{display:none!important}
.tabs{display:flex;gap:8px;margin:18px 0;flex-wrap:wrap}
.tabs button[aria-selected="true"]{border-color:#7bb7ed;background:#284b6a;font-weight:700}
.pin-map{width:100%;border-collapse:collapse;font-size:.9rem}
.pin-map td,.pin-map th{padding:6px;text-align:left;border-bottom:1px solid #334354}
</style></head><body>
<header><div><h1>RAMSES II Gateway</h1><small>ESP32-WROOM-32E · CC1101 · Controller (01:) + virtual room thermostat (12:) · passive RF diagnostics</small></div><span id="connection" class="muted">Connecting...</span></header>
<nav class="tabs" role="tablist" aria-label="Gateway pages"><button type="button" id="tab-diagnostics" role="tab" aria-selected="true" aria-controls="page-diagnostics">Diagnostics</button><button type="button" id="tab-settings" role="tab" aria-selected="false" aria-controls="page-settings">Settings</button></nav>
<div id="page-diagnostics" role="tabpanel" aria-labelledby="tab-diagnostics">
<section class="grid">
<div class="card">Radio / SPI<b id="radio">—</b><small id="chip"></small></div>
<div class="card">RAMSES packets<b id="frames">—</b><small id="errors"></small></div>
<div class="card">UART activity<b id="bytes">—</b><small id="sync"></small></div>
<div class="card">Wi-Fi / MQTT<b id="network">—</b><small id="ip"></small></div>
<div class="card">Free memory<b id="heap">—</b><small id="uptime"></small></div>
<div class="card">Receiver status<b id="health">—</b><small id="reset"></small></div>
</section>
<section class="panel"><h2>OpenTherm compatibility</h2>
<p class="muted">A scan transmits ONLY 3220 Read-Data requests; no OpenTherm writes or Evohome overrides. Transmission is disabled by default and requires explicit firmware configuration and authenticated access. Do not initiate before validating RF wiring and identifying your own R8810A.</p>
<div class="controls"><button id="ot-scan">Start read-only test</button>
<a href="/api/v1/opentherm/profile" target="_blank">Export compatibility JSON</a>
<span id="ot-state" class="muted">Loading...</span></div>
<pre id="ot-results">No compatibility scan requested.</pre>
<details class="panel" id="one-shot-panel">
<summary>Manual OpenTherm ID25 read test (unpaired experimental 18: or verified bound 01:)</summary>
<p class="muted">Send exactly ONE RAMSES 3220 Read-Data ID25 request (boiler flow temperature) at 868.300 MHz. A unique, unobserved 18: gateway identity can now request ID25 on standard 868 MHz hardware before pairing; verified bound 01: identities remain supported. The legacy 433 MHz cross-band path requires explicit experimental RX opt-in. No retries, polling, scan or heating writes. Minimum cooldown: 60 seconds.</p>
<p id="one-shot-status" class="muted">Checking bridge identity...</p>
<label class="setting-check"><input type="checkbox" id="one-shot-confirm"> I verified the R8810A RF ID and my unique gateway 18: identity, or confirmed the additional 01: controller binding. I authorize ONE manually triggered read-only RF transmission.</label>
<div class="controls"><label class="field">Single-test transmit method
<select id="one-shot-method" aria-label="One-shot transmit method">
<option value="uart">Original asynchronous UART / GDO0</option>
<option value="fifo">Reference FIFO (upstream bit-aligned, 60 RF bytes)</option>
</select></label><button type="button" id="one-shot-send" disabled>Transmit ONE Read-Data ID25</button><a href="/api/v1/opentherm/one-shot" target="_blank">One-shot diagnostic JSON</a></div>
<small>One click = exactly one RF request using the selected method; never both. Each method shares a fixed nominal -10 dBm 868 MHz PATABLE value. A 433M PCB's radiated power at 868 MHz remains unknown. Keep automatic scans/polling off.</small>
<p id="one-shot-result" aria-live="polite" class="muted">No request attempted.</p>
<pre id="one-shot-diag" aria-live="polite">TX diagnostic data appears after one request.</pre>
<pre id="one-shot-compare">UART/FIFO A-B comparison will appear here.</pre>
</details>
<details class="panel" id="pairing-panel">
<summary>Experimental R8810A pairing — additional virtual controller (01:)</summary>
<p class="muted">Experimental RAMSES 1FC9 binding. The <strong>Honeywell R8810A1018 device installation guide</strong>, page 2, says to press its button for <strong>5 seconds</strong> to enter binding mode: the red LED alternates 0.5 seconds ON / 0.5 seconds OFF. The manual does <strong>not</strong> require a preceding 15-second reset for this procedure. <strong>Do not clear existing bindings.</strong> Clicking the ESP32 button only queues an RF offer; it does not put the physical R8810A into binding mode. This manual documents a CM957 thermostat, not the safety or feasibility of adding a second virtual controller to an existing Evohome system. If the red LED does not flash after a deliberate 5-second press, stop the experiment and keep normal Evohome boiler control.</p>
<p id="pairing-status" class="muted">Waiting for an observed Evohome controller and OpenTherm bridge...</p>
<p id="pairing-blocker" aria-live="polite">Pairing preflight not yet checked.</p>
<div class="controls"><label class="field">Unused virtual controller numeric ID (01:NNNNNN, different from Evohome)
<input id="pairing-id" type="number" min="1" max="262142" step="1" placeholder="e.g. 62142" autocomplete="off"></label>
<a href="/api/v1/opentherm/pairing" target="_blank">Pairing JSON</a></div>
<label class="setting-check"><input id="pairing-confirm" type="checkbox"> I verified the R8810A and its existing Evohome controller ID, chose an unused 01: identity, put R8810A in 5-second bind mode without clearing existing bindings, and authorize one experimental 1FC9 bind transaction plus one read-only ID25 verification.</label>
<div class="controls"><button type="button" id="pairing-start" disabled>Send one pairing offer</button></div>
<label class="setting-check"><input type="checkbox" id="controller-forget-confirm"> I authorize stopping and forgetting ONLY the virtual 01: controller locally. This does NOT remove the R8810A binding or alter the existing Evohome controller.</label>
<div class="controls"><button type="button" id="controller-forget" disabled>Stop and forget virtual 01: locally</button></div>
<pre id="pairing-result" aria-live="polite">No pairing attempted. Confirmation sent is NOT proof of binding; only a directed 3220 reply validates data communication.</pre>
</details>
<details class="panel" id="thermostat-panel">
<summary>Virtual wireless room thermostat (12:) — pair with existing Evohome (01:)</summary>
<p class="muted">Independent of additional-controller (01:) pairing with the R8810A. Pair the virtual 12: room-temperature sensor to the existing Evohome controller, in its room/zone temperature-sensor binding mode. Existing R8810A control and controller pairing remain unchanged. A received 1FC9 controller acceptance is required before a confirmation or any 30C9 temperature report. The controller must already be observed on RF.</p>
<p class="muted">No automatic binding, setpoint changes or boiler commands. Changed temperatures are sent after a minimum 15-second RF spacing; unchanged readings are refreshed every 5 minutes. Old external measurements expire after 15 minutes. Saving a value is separate from RF transmission. RF delivery is not proven by a local TX completion. Controller acceptance alone does not prove the Evohome has assigned the correct zone sensor.</p>
<div class="controls">
<label class="field">Unused virtual thermostat ID (12:NNNNNN)
<input id="thermostat-id" type="number" min="1" max="262142" step="1" placeholder="e.g. 160002" autocomplete="off"></label>
<a href="/api/v1/thermostat" target="_blank">Thermostat JSON</a></div>
<label class="setting-check"><input id="thermostat-confirm" type="checkbox"> I have selected a temperature-sensor binding slot for the intended Evohome zone, and authorize one 12: sensor 1FC9 offer and one confirmation only if accepted.</label>
<div class="controls"><button type="button" id="thermostat-pair" disabled>Send thermostat pairing offer</button></div>
<label class="setting-check"><input type="checkbox" id="thermostat-forget-confirm"> I authorize stopping and forgetting ONLY the virtual 12: thermostat locally. This stops its RF temperature reports. It does NOT remove the sensor assignment stored on Evohome.</label>
<div class="controls"><button type="button" id="thermostat-forget" disabled>Stop and forget virtual 12: locally</button></div>
<div class="controls"><label class="field">External room temperature (°C; sent after acceptance)
<input id="thermostat-temp" type="number" min="-40" max="85" step="0.01" value="20.00"></label>
<button type="button" id="thermostat-set-temp" disabled>Update sensor reading</button></div>
<p class="muted">MQTT temperature topic is shown below. Publish &lt;Unix epoch seconds&gt;:&lt;integer centi-°C&gt;, for example &lt;current Unix timestamp&gt;:2150 for 21.50°C. Retained replay of an old measurement cannot refresh its 15-minute validity. Publish a new measurement with its actual sample timestamp at least every 15 minutes.</p>
<pre id="thermostat-status" aria-live="polite">No virtual thermostat pairing has been requested.</pre>
</details>
<section class="panel" aria-label="Passive radio receive diagnostics">
<h2>Passive radio receive diagnostics</h2>
<p class="muted">These counters include all RF senders and classify decoding errors before address checks. An invalid frame cannot be reliably attributed to the R8810A. The optional RX Debug switch logs one summary every 15 seconds; it does not change reception, filtering, TX or settings and resets to OFF after reboot.</p>
<label class="setting-check"><input type="checkbox" id="rx-debug-enable"> Enable detailed RX debug summaries (every 15 seconds, no restart)</label>
<span id="rx-debug-state" class="muted">Loading RX diagnostics...</span>
<div class="controls"><a href="/api/v1/radio/diagnostics" target="_blank">Receive diagnostic JSON</a><a href="/api/v1/radio/rx-health" target="_blank">RX health / MARCSTATE JSON</a><a href="/api/v1/debug/download" target="_blank">Export debug log</a></div>
<details id="rx-health-panel"><summary>CC1101 RX state and re-entry measurements (passive)</summary><pre id="rx-health">Collecting RX state samples...</pre></details>
<pre id="rx-debug-counters" aria-live="polite">Waiting for radio diagnostic counters...</pre>
<details class="panel" id="packet-capture-panel">
<summary>Extended RAMSES packet capture (received packets and rejected frames)</summary>
<p class="muted">Capture starts only when you click Start. Stores up to 192 RF decoder events in ESP32 RAM: every successfully decoded packet plus rejected frame candidates (with rejection reason and UART byte, but no reliable sender). Independent of the 12-packet Recent history and the web/MQTT queue. Automatically stops at 192; download before starting a new session, clearing, or rebooting. Not a raw RF/IQ capture and cannot show transmissions the CC1101 completely missed. No RF TX, reboot or saved settings.</p>
<div class="controls">
<button type="button" id="capture-start">Start new capture</button>
<button type="button" id="capture-stop">Stop capture</button>
<button type="button" id="capture-clear">Clear capture</button>
<a href="/api/v1/capture/download" target="_blank" id="capture-download">Download capture (.tsv)</a>
<a href="/api/v1/capture/status" target="_blank">Capture status JSON</a>
</div>
<p id="capture-state" class="muted" aria-live="polite">Capture is inactive.</p>
<pre id="capture-stats" aria-live="polite">No capture running.</pre>
</details>
</section>
</section>
</div><div id="page-settings" role="tabpanel" aria-labelledby="tab-settings" hidden>
<section class="panel"><h2>Gateway configuration</h2>
<p class="muted">Settings are saved on the ESP32 (NVS) and take precedence over the original GatewaySecrets.h defaults. Saving restarts the gateway. Leave password fields empty to KEEP existing passwords; secrets are never returned to the browser.</p>
<form id="cfg">
<fieldset><legend>Network</legend><div class="fields">
<label class="field">Gateway hostname<input name="hostname" id="cfg-hostname" maxlength="63" autocomplete="off"></label>
<label class="field">Wi-Fi SSID<input name="wifi_ssid" id="cfg-wifi_ssid" maxlength="32" autocomplete="off"></label>
<label class="field">Wi-Fi password (blank = unchanged)<input name="wifi_password" type="password" maxlength="64" autocomplete="new-password"></label>
<label class="field">MQTT broker / IP<input name="mqtt_host" id="cfg-mqtt_host" maxlength="128" autocomplete="off"></label>
<label class="field">MQTT port<input name="mqtt_port" id="cfg-mqtt_port" type="number" min="1" max="65535" required></label>
<label class="field">MQTT username<input name="mqtt_user" id="cfg-mqtt_user" maxlength="64" autocomplete="off"></label>
<label class="setting-check"><input type="checkbox" id="cfg-mqtt_discovery" name="mqtt_discovery"> Enable Home Assistant MQTT Discovery</label>
<label class="field">MQTT password (blank = unchanged)<input name="mqtt_password" type="password" maxlength="64" autocomplete="new-password"></label>
<label class="field">Timezone<select id="cfg-timezone" name="timezone">
<option value="Europe/Amsterdam">Europe/Amsterdam (CET/CEST)</option>
<option value="UTC">UTC</option><option value="Europe/London">Europe/London</option>
<option value="America/New_York">America/New_York</option>
<option value="Australia/Sydney">Australia/Sydney</option>
</select></label>
<div class="controls">
<label class="setting-check"><input type="checkbox" name="clear_wifi_password" id="cfg-clear_wifi_password"> Clear Wi-Fi password (open network)</label>
<label class="setting-check"><input type="checkbox" name="clear_mqtt_password" id="cfg-clear_mqtt_password"> Clear MQTT password</label>
</div></div></fieldset>
<fieldset><legend>Security and recovery</legend>
<div class="fields">
<label class="field">Web username<input id="cfg-web_user" name="web_user" maxlength="32" autocomplete="username"></label>
<label class="field">New web password (blank = unchanged)<input name="web_password" type="password" maxlength="64" autocomplete="new-password"></label>
<label class="field">Recovery AP password (blank = unchanged)<input name="recovery_password" type="password" maxlength="63" autocomplete="new-password"></label>
<label class="setting-check"><input type="checkbox" id="cfg-clear_recovery_password" name="clear_recovery_password"> Disable recovery AP (not recommended before changing Wi-Fi)</label>
</div><small>Web changes require existing HTTP authentication with a strong password. Use only a trusted LAN: HTTP Basic authentication is not encrypted. Wi-Fi credential updates are rolled back if the new network does not connect within 90 seconds.</small>
</fieldset>
<fieldset><legend>Radio and read-only OpenTherm</legend>
<div class="fields">
<label class="field">Module marking / PCB RF hardware<select id="cfg-rf_module_band" name="rf_module_band">
<option value="433">433 MHz (your photographed board)</option><option value="868">868 MHz</option>
<option value="315">315 MHz</option><option value="915">915 MHz</option></select></label>
<label class="field">Receiver frequency (kHz)<input id="cfg-rf_frequency_khz" name="rf_frequency_khz" type="number" min="300000" max="928000" list="rf-presets" required>
<datalist id="rf-presets"><option value="868300">Evohome 868.3 MHz</option><option value="433920">433.92 MHz</option></datalist></label>
<label class="setting-check"><input type="checkbox" id="cfg-rf_experimental_rx" name="rf_experimental_rx"> Allow experimental cross-band receive-only tuning</label>
<label class="field">R8810A RF ID (decimal portion of 10:XXXXXX)<input id="cfg-ot_bridge" name="ot_bridge" type="number" min="0" max="262143" required></label>
<label class="field">Virtual gateway RF ID (18:XXXXXX)<input id="cfg-ot_source" name="ot_source" type="number" min="0" max="262142" required></label>
</div>
<small id="cfg-rf-band">Software RF tuning does not guarantee reception on a PCB tuned for another band. Experimental RX can never transmit.</small>
<details><summary>ESP32 ↔ CC1101 wiring (fixed GPIO assignments)</summary><table class="pin-map"><thead><tr><th>CC1101</th><th>Signal</th><th>ESP32</th></tr></thead><tbody>
<tr><td>M-BUS 1</td><td>GND</td><td>GND</td></tr><tr><td>M-BUS 28</td><td>5V power (M5Stack M146 only)</td><td>5V / VIN</td></tr>
<tr><td>M-BUS 22</td><td>GDO0</td><td>GPIO26</td></tr><tr><td>M-BUS 8</td><td>CSN</td><td>GPIO5</td></tr>
<tr><td>M-BUS 11</td><td>SCK</td><td>GPIO18</td></tr><tr><td>M-BUS 7</td><td>MOSI</td><td>GPIO23</td></tr>
<tr><td>M-BUS 9</td><td>MISO</td><td>GPIO19</td></tr><tr><td>M-BUS 2</td><td>GDO2</td><td>GPIO27</td></tr>
</tbody></table><small>Confirmed M5Stack M146 868 MHz module: 5V to M-BUS 28 via the onboard regulator. A bare CC1101 breakout requires 3.3V instead. Disconnect power before changing wiring.</small></details>
<div class="controls">
<label class="setting-check"><input name="ot_scan_all" id="cfg-ot_scan_all" type="checkbox"> Scan all 256 Data-IDs</label>
<label class="setting-check"><input name="ot_auto_poll" id="cfg-ot_auto_poll" type="checkbox"> Automatic read polling</label>
<label class="setting-check"><input name="ot_tx_enabled" id="cfg-ot_tx_enabled" type="checkbox"> Enable RF read-request transmission</label>
</div><label class="setting-check"><input type="checkbox" name="rf_safety_confirm" id="cfg-rf_safety_confirm"> I confirm the CC1101 radio is physically connected, correct 868 MHz hardware is installed, and RF identities have been checked. No boiler writes are supported.</label>
</fieldset>
<div class="controls"><button type="submit" id="cfg-submit">Save settings and restart</button><span id="cfg-message" aria-live="polite" class="muted"></span></div>
</form></section>
</div><div id="page-diagnostics-log">
<section class="panel"><h2>Diagnostic log</h2>
<div class="controls"><label>Severity <select id="level"><option value="2" selected>Info, warnings &amp; errors</option><option value="3">Include trace</option><option value="1">Warnings &amp; errors</option><option value="0">Errors only</option></select></label><button id="clear">Clear view</button><a href="/api/v1/debug/download" target="_blank">Export log</a><a href="/api/v1/status" target="_blank">JSON status</a></div>
<pre id="logs" aria-live="polite">Waiting for gateway events...</pre>
</section>
</div><p class="muted">This page polls every 2 seconds. RF read requests are transmitted ONLY after manual enabling of the explicitly guarded diagnostic mode; no setpoint or heating-control writes are available. Keep the gateway on a trusted local network; do not expose HTTP port 80 to the Internet.</p>
<script defer src="/gateway.js"></script></body></html>)GWHTML";
