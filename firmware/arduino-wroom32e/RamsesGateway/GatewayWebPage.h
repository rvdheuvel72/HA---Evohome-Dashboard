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
</style></head><body>
<header><div><h1>RAMSES II Gateway</h1><small>ESP32-WROOM-32E · CC1101 · read-only OpenTherm diagnostics</small></div><span id="connection" class="muted">Connecting...</span></header>
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
</section>
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
<label class="field">CC1101 frequency (kHz)<input id="cfg-rf_frequency_khz" name="rf_frequency_khz" type="number" min="300000" max="928000" required></label>
<label class="field">R8810A RF ID (decimal portion of 10:XXXXXX)<input id="cfg-ot_bridge" name="ot_bridge" type="number" min="0" max="262143" required></label>
<label class="field">Virtual gateway RF ID (18:XXXXXX)<input id="cfg-ot_source" name="ot_source" type="number" min="0" max="262142" required></label>
</div>
<small id="cfg-rf-band">Hardware module band is immutable after firmware build.</small>
<div class="controls">
<label class="setting-check"><input name="ot_scan_all" id="cfg-ot_scan_all" type="checkbox"> Scan all 256 Data-IDs</label>
<label class="setting-check"><input name="ot_auto_poll" id="cfg-ot_auto_poll" type="checkbox"> Automatic read polling</label>
<label class="setting-check"><input name="ot_tx_enabled" id="cfg-ot_tx_enabled" type="checkbox"> Enable RF read-request transmission</label>
</div><label class="setting-check"><input type="checkbox" name="rf_safety_confirm" id="cfg-rf_safety_confirm"> I confirm the CC1101 radio is physically connected, correct 868 MHz hardware is installed, and RF identities have been checked. No boiler writes are supported.</label>
</fieldset>
<div class="controls"><button type="submit" id="cfg-submit">Save settings and restart</button><span id="cfg-message" aria-live="polite" class="muted"></span></div>
</form></section>
<section class="panel"><h2>Diagnostic log</h2>
<div class="controls"><label>Severity <select id="level"><option value="2" selected>Info, warnings &amp; errors</option><option value="3">Include trace</option><option value="1">Warnings &amp; errors</option><option value="0">Errors only</option></select></label><button id="clear">Clear view</button><a href="/api/v1/debug/download" target="_blank">Export log</a><a href="/api/v1/status" target="_blank">JSON status</a></div>
<pre id="logs" aria-live="polite">Waiting for gateway events...</pre>
</section>
<p class="muted">This page polls every 2 seconds. RF read requests are transmitted ONLY after manual enabling of the explicitly guarded diagnostic mode; no setpoint or heating-control writes are available. Keep the gateway on a trusted local network; do not expose HTTP port 80 to the Internet.</p>
<script>
const el=id=>document.getElementById(id);
async function loadConfig(){
 try{
  const response=await fetch('/api/v1/config',{cache:'no-store'});
  if(!response.ok)throw new Error('HTTP '+response.status);
  const values=await response.json();
  for(const name of ['hostname','wifi_ssid','mqtt_host','mqtt_port',
    'mqtt_user','ot_bridge','ot_source','rf_frequency_khz','web_user']){
    const field=el('cfg-'+name);
    if(field && values[name]!==undefined)field.value=values[name];
  }
  el('cfg-timezone').value=values.timezone||'Europe/Amsterdam';
  for(const name of ['mqtt_discovery','ot_scan_all','ot_auto_poll','ot_tx_enabled'])
    el('cfg-'+name).checked=!!values[name];
  set('cfg-rf-band','CC1101 hardware module band: '+values.rf_module_band+
    ' MHz · frequency set after next restart · Radio detection not required to edit');
 }catch(e){set('cfg-message','Cannot load configuration: '+e.message)}
}
el('cfg').addEventListener('submit',async event=>{
 event.preventDefault();
 const button=el('cfg-submit');button.disabled=true;
 const data=new URLSearchParams(new FormData(el('cfg')));
 for(const name of ['mqtt_discovery','ot_scan_all','ot_auto_poll','ot_tx_enabled','rf_safety_confirm',
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
