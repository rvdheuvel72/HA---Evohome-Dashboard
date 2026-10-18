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
<header><div><h1>RAMSES II Gateway</h1><small>ESP32-WROOM-32E · CC1101 · OpenTherm read diagnostics · optional experimental pairing</small></div><span id="connection" class="muted">Connecting...</span></header>
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
<p class="muted">Send exactly ONE RAMSES 3220 Read-Data ID25 request (boiler flow temperature) at 868.300 MHz. A verified additional controller 01: identity can be used on standard 868 MHz hardware; the pre-existing 433M cross-band unpaired test remains separately guarded. No retries, polling, scan or heating writes. Minimum cooldown: 60 seconds.</p>
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
<details class="panel" id="tx-loopback-panel">
<summary>ESP32 UART TX loopback — GPIO26 → GPIO25 (NO RF transmit)</summary>
<p class="muted">Bench wiring: connect ESP32 GPIO26 to ESP32 GPIO25. You may leave CC1101 GDO0 disconnected as in your current test. This diagnostic sends a fixed 82-byte encoded OpenTherm RAMSES 1FC9 test frame over the ESP32 UART TX pin only, reads the electrical bytes through UART2 RX on GPIO25, and compares and decodes them. It DOES NOT enter CC1101 transmit mode, transmit an RF packet, bind to R8810A, or affect stored settings.</p>
<p class="muted">The CC1101 must be in receive mode with automatic polling, scanning and RF TX disabled, and no other RF transaction pending. Do not use the R8810A pairing button for this test. Restore GPIO26 → CC1101 GDO0 before testing actual RF later.</p>
<label class="setting-check"><input type="checkbox" id="loopback-confirm"> GPIO26 is jumpered to GPIO25 and I authorize ONE GPIO-only UART transmission; no RF test or pairing.</label>
<div class="controls"><button type="button" id="loopback-start" disabled>Test GPIO26 → GPIO25</button><a href="/api/v1/radio/tx-loopback" target="_blank">Loopback JSON</a></div>
<pre id="loopback-result" aria-live="polite">No GPIO loopback attempted.</pre>
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
<pre id="pairing-result" aria-live="polite">No pairing attempted. Confirmation sent is NOT proof of binding; only a directed 3220 reply validates data communication.</pre>
</details>
<section class="panel" aria-label="Passive radio receive diagnostics">
<h2>Passive radio receive diagnostics</h2>
<p class="muted">These counters include all RF senders and classify decoding errors before address checks. An invalid frame cannot be reliably attributed to the R8810A. The optional RX Debug switch logs one summary every 15 seconds; it does not change reception, filtering, TX or settings and resets to OFF after reboot.</p>
<label class="setting-check"><input type="checkbox" id="rx-debug-enable"> Enable detailed RX debug summaries (every 15 seconds, no restart)</label>
<span id="rx-debug-state" class="muted">Loading RX diagnostics...</span>
<div class="controls"><a href="/api/v1/radio/diagnostics" target="_blank">Receive diagnostic JSON</a><a href="/api/v1/debug/download" target="_blank">Export debug log</a></div>
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
<tr><td>1</td><td>GND (brown)</td><td>GND</td></tr><tr><td>2</td><td>VCC (red)</td><td>3V3 only</td></tr>
<tr><td>3</td><td>GDO0</td><td>GPIO26</td></tr><tr><td>4</td><td>CSN</td><td>GPIO5</td></tr>
<tr><td>5</td><td>SCK</td><td>GPIO18</td></tr><tr><td>6</td><td>MOSI</td><td>GPIO23</td></tr>
<tr><td>7</td><td>MISO</td><td>GPIO19</td></tr><tr><td>8</td><td>GDO2</td><td>GPIO27</td></tr>
</tbody></table><small>Pin numbering for your photographed V2.0 board (component side toward you, antenna at top). GPIO mapping is fixed in firmware.</small></details>
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
<script>
const el=id=>document.getElementById(id);
function showPage(page){
 const selected=page==='settings'?'settings':'diagnostics';
 for(const name of ['diagnostics','settings']){
  const active=name===selected;
  el('tab-'+name).setAttribute('aria-selected',String(active));
  el('page-'+name).hidden=!active;
 }
 el('page-diagnostics-log').hidden=selected!=='diagnostics';
 if(location.hash!=='#'+selected)history.replaceState(null,'','#'+selected);
}
for(const name of ['diagnostics','settings'])
 el('tab-'+name).addEventListener('click',()=>showPage(name));
window.addEventListener('hashchange',()=>showPage(location.hash==='#settings'?'settings':'diagnostics'));
showPage(location.hash==='#settings'?'settings':'diagnostics');

async function loadConfig(){
 try{
  const response=await fetch('/api/v1/config',{cache:'no-store'});
  if(!response.ok)throw new Error('HTTP '+response.status);
  const values=await response.json();
  for(const name of ['hostname','wifi_ssid','mqtt_host','mqtt_port',
    'mqtt_user','ot_bridge','ot_source','rf_frequency_khz','rf_module_band','web_user']){
    const field=el('cfg-'+name);
    if(field && values[name]!==undefined)field.value=values[name];
  }
  el('cfg-timezone').value=values.timezone||'Europe/Amsterdam';
  for(const name of ['mqtt_discovery','ot_scan_all','ot_auto_poll','ot_tx_enabled','rf_experimental_rx'])
    el('cfg-'+name).checked=!!values[name];
  set('cfg-rf-band','Declared PCB marking: '+values.rf_module_band+
    ' MHz · applied after restart · cross-band 868 MHz RX on 433 MHz board is experimental');
 }catch(e){set('cfg-message','Cannot load configuration: '+e.message)}
}
el('cfg').addEventListener('submit',async event=>{
 event.preventDefault();
 const button=el('cfg-submit');button.disabled=true;
 const data=new URLSearchParams(new FormData(el('cfg')));
 const band=Number(el('cfg-rf_module_band').value),khz=Number(el('cfg-rf_frequency_khz').value);
 const matched=(band===315&&khz>=300000&&khz<=348000)||
   (band===433&&khz>=387000&&khz<=464000)||
   (band===868&&khz>=863000&&khz<=870000)||
   (band===915&&khz>=902000&&khz<=928000);
 if(!matched && !el('cfg-rf_experimental_rx').checked){
   set('cfg-message','Enable experimental cross-band RX for a frequency outside the module marking.');
   button.disabled=false;return;
 }
 if(el('cfg-ot_tx_enabled').checked && el('cfg-rf_experimental_rx').checked){
   set('cfg-message','Disable RF transmission for experimental RX testing.');
   button.disabled=false;return;
 }
 for(const name of ['mqtt_discovery','ot_scan_all','ot_auto_poll','ot_tx_enabled','rf_experimental_rx','rf_safety_confirm',
    'clear_wifi_password','clear_mqtt_password','clear_recovery_password'])
   data.set(name,el('cfg-'+name).checked?'1':'0');
 try{
  const response=await fetch('/api/v1/config',{
   method:'POST',
   headers:{'X-Gateway-Config':'save',
            'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},
   body:data.toString()
  });
  const result=await response.json();
  if(!response.ok)throw new Error(result.error||('HTTP '+response.status));
  set('cfg-message','Saved. ESP32 restarting — reconnect using its updated IP/hostname in a few seconds.');
 }catch(e){set('cfg-message','Save rejected: '+e.message);button.disabled=false}
});

async function updatePacketCapture(){
  try {
    const response=await fetch('/api/v1/capture/status',{cache:'no-store'});
    if(!response.ok)throw new Error('HTTP '+response.status);
    const p=await response.json();
    set('capture-state',
      p.full?'Buffer FULL — stopped automatically. Download now.':
      p.active?'CAPTURING — download/stop available':'Capture stopped');
    el('capture-stop').disabled=!p.active;
    el('capture-download').style.pointerEvents=p.count>0?'auto':'none';
    set('capture-stats',
      p.count+' / '+p.limit+' entries · '+p.decoded+' valid decoded · '+
      p.rejected+' rejected frames'+
      '\nReceiver syncs during session: '+p.syncs_since_start+
      '\nRF UART bytes: '+p.radio_uart_bytes_since_start+
      '\nWeb/MQTT queue drops: '+p.main_queue_dropped_since_start+
      '\nCapture is RAM-only; start or clear discards unsaved entries.');
  }catch(e){set('capture-state','Capture status unavailable: '+e.message)}
}
async function changePacketCapture(action) {
  if((action==='clear'||action==='start') && !confirm('Unsaved capture entries will be lost. '+(action==='start'?'Start a new session?':'Clear current session?')))return;
  const button=el('capture-'+action);button.disabled=true;
  try {
    const response=await fetch('/api/v1/capture/action',{
      method:'POST',
      headers:{'X-Gateway-Config':'packet-capture',
               'Content-Type':'application/x-www-form-urlencoded'},
      body:'action='+action
    });
    const data=await response.json();
    if(!response.ok)throw new Error(data.error||'HTTP '+response.status);
  }catch(e){set('capture-state','Capture action failed: '+e.message)}
  finally{button.disabled=false;updatePacketCapture()}
}
for(const action of ['start','stop','clear'])
  el('capture-'+action).addEventListener('click',()=>changePacketCapture(action));
updatePacketCapture();setInterval(updatePacketCapture,5000);

async function updateRxDiagnostics(){
  try {
    const response=await fetch('/api/v1/radio/diagnostics',{cache:'no-store'});
    if(!response.ok)throw new Error('HTTP '+response.status);
    const d=await response.json();
    el('rx-debug-enable').checked=!!d.debug_enabled;
    set('rx-debug-state',d.debug_enabled?'RX Debug ON (15s summaries)':'RX Debug OFF (passive counters still available)');
    const r=d.rejected||{};
    const age=t=>t?Math.round(t/1000)+'s':'not observed';
    set('rx-debug-counters',
      'R8810A requests observed: '+d.requests_to_bridge+
      '\\nR8810A messages received: '+d.messages_from_bridge+
      ' · 3EF0='+d.bridge_3ef0+' · 3220='+d.bridge_3220+
      '\\nLast controller request age: '+age(d.last_request_uptime_ms&&d.last_request_age_ms)+
      '\\nLast bridge message age: '+age(d.last_bridge_uptime_ms&&d.last_bridge_age_ms)+
      '\\nFrame rejection reasons (all senders):'+
      '\\n  Checksum '+r.checksum+' · Manchester '+r.invalid_manchester+
      ' · Malformed '+r.malformed_structure+' · Oversize '+r.oversized+
      ' · Incomplete '+r.incomplete+' · Idle timeout '+r.idle_timeout+
      ' · Format '+r.format_failure+
      '\\nLast error: '+d.last_rejection+
      ' · RSSI magnitude (indicative): '+d.last_rssi_magnitude+
      '\\nInvalid RF frames cannot identify their sender.');
  }catch(e){set('rx-debug-state','RX diagnostics unavailable: '+e.message)}
}
el('rx-debug-enable').addEventListener('change',async()=>{
  const checkbox=el('rx-debug-enable');
  const enabled=checkbox.checked;
  checkbox.disabled=true;
  try {
    const response=await fetch('/api/v1/radio/diagnostics',{
      method:'POST',
      headers:{'X-Gateway-Config':'radio-debug','Content-Type':'application/x-www-form-urlencoded'},
      body:'enabled='+(enabled?'1':'0')
    });
    const data=await response.json();
    if(!response.ok)throw new Error(data.error||'HTTP '+response.status);
  }catch(e){set('rx-debug-state','Cannot switch RX debug: '+e.message)}
  finally{checkbox.disabled=false;updateRxDiagnostics()}
});
updateRxDiagnostics();setInterval(updateRxDiagnostics,5000);

async function otStatus(){
 try{
  const response=await fetch('/api/v1/opentherm/profile',{cache:'no-store'});
  if(!response.ok)throw new Error('HTTP '+response.status);
  const p=await response.json();
  el('ot-scan').disabled=!p.rf_tx_enabled||p.scan_active||p.scan_pending;
  set('ot-state',(p.scan_active?'Running':p.scan_completed?'Completed':'Not tested')+
    ' · '+p.attempted+' attempts · '+p.read_ack+' Read-Ack · '+p.frequency_khz+' kHz'+
    (p.rf_tx_enabled?'':' · RF TX disabled'));
  const lines=(p.results||[]).map(x=>'ID '+String(x.id).padStart(3,' ')+
    ' '+x.name.padEnd(25)+' '+x.result+
    (x.value===null?'':' · '+x.value)+
    (x.auto_poll?' · eligible for polling':''));
  set('ot-results',lines.length?lines.join('\n'):'No OpenTherm replies recorded.');
 }catch(e){set('ot-state','Profile unavailable: '+e.message)}
}
const pairingBlockMessages={
  configuration_unavailable:'Gateway configuration has not loaded.',
  configure_bridge_id:'Settings: enter the numeric RF address of YOUR observed R8810A (10:xxxxxx, without the 10: prefix), not another installation\'s ID.',
  bridge_not_observed:'The R8810A has not yet been heard. Wait for valid RAMSES traffic.',
  evohome_controller_not_observed:'The normal Evohome controller has not yet been observed talking to the R8810A. Wait for normal boiler RF traffic.',
  evohome_controller_ambiguous:'Conflicting controller IDs were observed in R8810A 3150 traffic (a retained virtual controller may be involved). Pairing is blocked until an unambiguous direct Evohome-to-R8810A request is received. Do not erase existing bindings.',
  automatic_tx_enabled:'Settings: turn automatic OpenTherm RF transmission OFF.',
  automatic_polling_enabled:'Settings: turn automatic OpenTherm polling OFF.',
  full_scan_enabled:'Settings: turn full OpenTherm scan OFF.',
  gateway_source_collision:'Your configured 18: gateway source ID was observed on the network. Choose a unique ID; do not use the Evohome ID.',
  radio_configuration_not_supported:'RF configuration is not accepted for this experimental pairing mode.',
  controller_id_coverage_exhausted:'More controller IDs have been seen than can safely be tracked. Pairing is blocked to avoid ID collisions.',
  opentherm_scan_active:'An OpenTherm scan is already active; stop it before pairing.',
  opentherm_scan_requested:'An OpenTherm scan is queued. Wait for it to finish.',
  opentherm_one_shot_active:'An OpenTherm read test is running. Wait for it to finish.',
  pairing_already_running:'A pairing transaction is already underway.',
  pairing_cooldown:'A previous pairing attempt is cooling down. Wait for the timer.',
  radio_not_ready:'The CC1101 radio is not initialized. Check the radio diagnostic panel.'
};
let pairingPostError='';
let loopbackPostError='';
async function updateLoopback(){
 try {
  const r=await fetch('/api/v1/radio/tx-loopback',{cache:'no-store'});
  if(!r.ok)throw new Error('HTTP '+r.status);
  const d=await r.json();
  el('loopback-start').disabled=!d.ready||!el('loopback-confirm').checked;
  set('loopback-result',
    'Status: '+d.status+(loopbackPostError?' · '+loopbackPostError:'')+
    '\nLocal UART: '+d.uart_written_bytes+'/'+d.expected_bytes+' sent · '+d.captured_bytes+' captured'+
    (d.first_mismatch_index>=0?' · first byte mismatch at '+d.first_mismatch_index:'')+
    '\n1FC9 decoded: '+d.decoded_1fc9+' · UART '+d.uart_elapsed_us+' µs · total '+d.capture_elapsed_us+' µs'+
    '\nCC1101 MARCSTATE before/after: '+d.radio_marc_before+'/'+d.radio_marc_after+
    '\nExpected hex: '+(d.expected_hex||'—')+
    '\nCaptured hex: '+(d.captured_hex||'—')+
    '\nOnly verifies electrical pin-to-pin UART; NOT an RF or R8810A acknowledgment.');
 }catch(e){
  el('loopback-start').disabled=true;
  set('loopback-result','GPIO loopback status unavailable: '+e.message);
 }
}
el('loopback-confirm').addEventListener('change',updateLoopback);
el('loopback-start').addEventListener('click',async()=>{
 el('loopback-start').disabled=true;
 try{
  const r=await fetch('/api/v1/radio/tx-loopback',{
   method:'POST',
   headers:{'X-Gateway-Config':'tx-loopback','Content-Type':'application/x-www-form-urlencoded'},
   body:new URLSearchParams({confirm:'gpio26-gpio25-no-rf'}).toString()
  });
  const d=await r.json();
  if(!r.ok)throw new Error(d.error||'HTTP '+r.status);
  el('loopback-confirm').checked=false;
  loopbackPostError='';
  set('loopback-result','GPIO loopback queued. Reading GPIO25; NO RF transmission...');
 }catch(e){
  el('loopback-confirm').checked=false;
  loopbackPostError='Test rejected: '+e.message;
 }
 finally{updateLoopback()}
});
updateLoopback();setInterval(updateLoopback,2500);

async function updatePairing(){
 try{
  const r=await fetch('/api/v1/opentherm/pairing',{cache:'no-store'});
  if(!r.ok)throw new Error('HTTP '+r.status);
  const x=await r.json();
  const entered=el('pairing-id').value.trim();
  const candidate=Number(entered);
  const valid=entered.length>0 && Number.isInteger(candidate)&&
    candidate>0&&candidate<262143&&
    candidate!==Number(x.existing_controller_id);
  const active=['offer_queued','waiting_for_1fc9_response',
    'confirmation_queued','verification_queued','waiting_for_3220_reply'].includes(x.state);
  const blockers=[];
  if(!x.ready)blockers.push(pairingBlockMessages[x.block_reason]||
      'Gateway rejected pairing preflight: '+(x.block_reason||'unknown'));
  if(active && x.ready)blockers.push('Pairing already in progress.');
  if(x.cooldown_ms>0 && x.block_reason!=='pairing_cooldown')
    blockers.push('Wait '+Math.ceil(x.cooldown_ms/1000)+'s before retrying.');
  if(!valid)blockers.push('Enter a NEW unused 01: ID (1–262142), different from the observed Evohome controller.');
  if(!el('pairing-confirm').checked)blockers.push('Tick the explicit authorization checkbox after checking the physical bridge.');
  el('pairing-start').disabled=blockers.length>0;
  set('pairing-blocker',(pairingPostError?pairingPostError+' ':'')+
    (blockers.length?'Pairing blocked: '+blockers.join(' '):
      'Software preflight READY. Send only if the R8810A red LED is visibly flashing in binding mode.'));
  set('pairing-status',
    'R8810A 10:'+String(x.bridge_id).padStart(6,'0')+
    ' · observed Evohome 01:'+String(x.existing_controller_id).padStart(6,'0')+
    ' · '+(x.controller_id_coverage_exhausted?'PAIRING BLOCKED: too many distinct 01: controllers observed; ID collision coverage exhausted (reboot after confirming identity)':
      x.ready?'READY for deliberate bind':'Not ready: observe controller & bridge; disable scan/poll/automatic TX')+
    ' · controller IDs observed '+x.controller_ids_seen+
    ' · cooldown '+Math.ceil(x.cooldown_ms/1000)+'s');
  set('pairing-result','State: '+x.state+
    '\\n1FC9 response: '+x.offer_accepted+
    ' · confirmation sent: '+x.confirmation_sent+
    '\\nDirected OpenTherm reply verified: '+x.verified_reply+
    ' · Read-Ack: '+x.read_ack+
    '\\nTX last step: '+x.tx_step+' · '+x.tx_phase+' · '+x.tx_bytes+'/'+x.tx_expected_bytes+' bytes'+
    '\\nLOCAL TX VERIFIED: '+(x.tx_local_verified?'YES':'NO')+' · '+x.tx_local_verification_reason+
    '\\nUART: '+x.tx_uart_elapsed_us+' µs (minimum '+x.tx_uart_min_expected_us+' µs) · total '+x.tx_elapsed_us+' µs'+
    '\\nFrequency synthesizer expected/readback: 0x'+Number(x.tx_frequency_expected||0).toString(16).toUpperCase()+
      ' / 0x'+Number(x.tx_frequency_readback||0).toString(16).toUpperCase()+
    '\\nCC1101 PKTCTRL0='+x.tx_pktctrl0+' IOCFG0='+x.tx_iocfg0+
      ' MDMCFG4/3/2='+x.tx_mdmcfg4+'/'+x.tx_mdmcfg3+'/'+x.tx_mdmcfg2+
      ' PA='+x.tx_pa_expected+'/'+x.tx_pa_readback+
    '\\nMARCSTATE before/TX/after UART/after RX: '+x.tx_marc_before+'/'+x.tx_marc_during+'/'+x.tx_marc_after_uart+'/'+x.tx_marc_after+
      ' · RX confirmed '+x.tx_rx_state_confirmed+
    '\\nThis is local radio/UART verification only: not proof of RF delivery, physical GPIO wire or R8810A acceptance.');
 }catch(e){
  el('pairing-start').disabled=true;
  set('pairing-status','Pairing status unavailable: '+e.message);
  set('pairing-blocker','Pairing blocked: cannot read /api/v1/opentherm/pairing. Check firmware version, gateway connectivity and web authentication.');
 }
}
el('pairing-confirm').addEventListener('change',()=>{pairingPostError='';updatePairing()});
el('pairing-id').addEventListener('input',()=>{pairingPostError='';updatePairing()});
el('pairing-start').addEventListener('click',async()=>{
 el('pairing-start').disabled=true;
 try{
  const data=new URLSearchParams({
    virtual_controller_id:el('pairing-id').value,
    confirm:'manual-1fc9-additional-controller'
  });
  const r=await fetch('/api/v1/opentherm/pairing',{
   method:'POST',
   headers:{'X-Gateway-Config':'opentherm-pairing',
            'Content-Type':'application/x-www-form-urlencoded'},
   body:data.toString()
  });
  const reply=await r.json();
  if(!r.ok)throw new Error(reply.error||'HTTP '+r.status);
  el('pairing-confirm').checked=false;
  pairingPostError='';
  set('pairing-result','1FC9 offer queued. Waiting for bridge response...');
 }catch(e){
  el('pairing-confirm').checked=false;
  pairingPostError='Pairing request rejected: '+e.message+'.';
  set('pairing-blocker',pairingPostError);
 }
 finally{updatePairing()}
});
updatePairing();setInterval(updatePairing,5000);

async function updateOneShot(){
 try{
  const r=await fetch('/api/v1/opentherm/one-shot',{cache:'no-store'});
  if(!r.ok)throw new Error('HTTP '+r.status);
  const p=await r.json();
  const available=p.ready && !['queued','awaiting_reply'].includes(p.status) &&
    Number(p.cooldown_ms)===0;
  el('one-shot-send').disabled=!el('one-shot-confirm').checked||!available;
  set('one-shot-status','Bridge '+(p.bridge_seen?'observed':'NOT yet observed')+
    ' · 18: source '+(p.source_conflict?'ADDRESS CONFLICT':'no observed conflict')+
    ' · cooldown '+Math.ceil(Number(p.cooldown_ms)/1000)+'s'+
    (p.ready?' · ready':' · set R8810A and unique virtual RF IDs in Settings, with scans, polling and normal TX off'));
  set('one-shot-result','ID25: '+p.status+' · method '+p.tx_method+
    (p.has_value?' · flow '+p.value_c+' °C':'')+
    (p.attempted?' · raw '+p.raw:''));
  const hex=n=>'0x'+Number(n||0).toString(16).toUpperCase().padStart(2,'0');
  set('one-shot-diag',
    'Local TX: '+p.tx_phase+' · '+(p.tx_local_complete?'sequence completed':'no confirmed local completion')+
    '\\nData: '+p.tx_written_bytes+'/'+p.tx_expected_bytes+' bytes · elapsed '+p.tx_elapsed_us+' µs'+
    '\\nPA configured/readback: '+hex(p.tx_pa_programmed)+' / '+hex(p.tx_pa_readback)+
    '\\nMARCSTATE before/during/after: '+hex(p.tx_marc_before)+' / '+hex(p.tx_marc_during)+' / '+hex(p.tx_marc_after)+
    '\\nFIFO count before/after: '+hex(p.tx_fifo_before)+' / '+hex(p.tx_fifo_after)+
    '\\nSaw TX state: '+p.tx_saw_tx+' · RX restored: '+p.tx_rx_restored+
    '\\nThis does NOT establish over-the-air delivery.');
  set('one-shot-compare',(p.comparison||[]).map(x=>
    x.method+' · '+x.attempts+' attempts · local done '+x.local_completed+
    ' · ACK '+x.read_acks+' · timeout '+x.timeouts+
    ' · other '+x.other_replies+' · TX failure '+x.tx_failures+
    ' · last '+x.last_phase+' ('+x.last_elapsed_us+' µs)').join('\\n'));
 }catch(e){
  el('one-shot-send').disabled=true;
  set('one-shot-status','One-shot status unavailable: '+e.message);
 }
}
el('one-shot-confirm').addEventListener('change',updateOneShot);
el('one-shot-method').addEventListener('change',updateOneShot);
el('one-shot-send').addEventListener('click',async()=>{
 el('one-shot-send').disabled=true;
 try{
  const r=await fetch('/api/v1/opentherm/one-shot',{
   method:'POST',
   headers:{'X-Gateway-Config':'one-shot','Content-Type':'application/x-www-form-urlencoded'},
   body:'confirm=experimental-single-read-25&method='+encodeURIComponent(el('one-shot-method').value)
  });
  const body=await r.json();
  if(!r.ok)throw new Error(body.error||('HTTP '+r.status));
  el('one-shot-confirm').checked=false;
  set('one-shot-result','One request queued. Waiting for R8810A reply...');
 }catch(e){set('one-shot-result','Request rejected: '+e.message)}
 finally{updateOneShot()}
});
updateOneShot();setInterval(updateOneShot,5000);
el('ot-scan').addEventListener('click',async()=>{
 el('ot-scan').disabled=true;
 try{
  const r=await fetch('/api/v1/opentherm/scan',{
   method:'POST',headers:{'X-Gateway-Config':'scan'}
  });
  const body=await r.json();
  if(!r.ok)throw new Error(body.error||'HTTP '+r.status);
  set('ot-state','Read-only scan queued');
 }catch(e){set('ot-state','Cannot start: '+e.message)}
 finally{otStatus()}
});

let cursor=0,lines=[],working=false;
const number=n=>Number(n||0).toLocaleString('en-US');
function set(id,value){el(id).textContent=value}
function add(line){lines.push(line);if(lines.length>250)lines.shift();set('logs',lines.join('\n'))}
async function poll(){
if(working)return;working=true;
try{
const status=await fetch('/api/v1/status',{cache:'no-store'});
if(!status.ok)throw new Error('status HTTP '+status.status);
const s=await status.json();
set('connection','Online');el('connection').className='ok';
set('radio',s.radio_detected?'Detected':'Not detected');
set('chip','Part 0x'+Number(s.cc1101_part||0).toString(16)+' · Version 0x'+Number(s.cc1101_version||0).toString(16));
set('frames',number(s.valid_frames));
set('errors',number(s.rejected_frames)+' rejected · '+number(s.dropped_frames)+' dropped');
set('bytes',number(s.rf_uart_bytes));
set('sync',number(s.rf_syncs)+' synchronization sequences');
set('network',(s.wifi_connected?'Wi-Fi up':'Wi-Fi down')+' / '+(s.mqtt_connected?'MQTT up':'MQTT down'));
set('ip','Host '+(s.hostname||'—')+' · mDNS '+(s.mdns_active?'active':'off')+' · Wi-Fi '+(s.ip||'—')+' · AP '+(s.ap_ip||'off'));
set('heap',number(s.free_heap)+' B');
set('uptime',Math.floor((s.uptime_ms||0)/1000)+' seconds up');
set('health',s.radio_detected?(s.valid_frames?'Receiving':'Waiting for RF'):'Radio not connected');
set('reset','Last reset: '+(s.reset_reason||'unknown'));
const response=await fetch('/api/v1/debug/logs?since='+cursor+'&level='+el('level').value,{cache:'no-store'});
if(!response.ok)throw new Error('logs HTTP '+response.status);
const data=await response.json();
if(cursor===0 && lines.length===0)set('logs','');
for(const item of data.entries||[]){
 const timestamp=item.timestamp||('+'+Math.floor(item.uptime_ms/1000)+'s (clock waiting for NTP)');
 add('['+timestamp+'] '+item.level.padEnd(5)+' '+item.message);
}
cursor=Number(data.next||cursor);
}catch(error){set('connection','Disconnected');el('connection').className='bad';if(lines.length===0)set('logs','Network error: '+error.message)}
finally{working=false}
}
el('clear').addEventListener('click',()=>{lines=[];set('logs','');cursor=0;poll()});
el('level').addEventListener('change',()=>{lines=[];set('logs','');cursor=0;poll()});
poll();setInterval(poll,2000);
otStatus();setInterval(otStatus,5000);
loadConfig();
</script></body></html>)GWHTML";
