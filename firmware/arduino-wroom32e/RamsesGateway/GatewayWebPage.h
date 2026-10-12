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
label{display:inline-flex;gap:5px;align-items:center}select,button{background:#25384b;color:#edf4ff;border:1px solid #536c85;border-radius:6px;padding:7px;cursor:pointer}
pre{font-size:12px;white-space:pre-wrap;overflow-wrap:anywhere;max-height:420px;overflow:auto;line-height:1.45}
.controls{display:flex;align-items:center;flex-wrap:wrap;gap:10px;margin-bottom:8px}
.ok{color:#7be2a2}.bad{color:#ff9f9b}
a{color:#a9d5ff}
</style></head><body>
<header><div><h1>RAMSES II Gateway</h1><small>ESP32-WROOM-32E · CC1101 · read-only diagnostics</small></div><span id="connection" class="muted">Connecting...</span></header>
<section class="grid">
<div class="card">Radio / SPI<b id="radio">—</b><small id="chip"></small></div>
<div class="card">RAMSES packets<b id="frames">—</b><small id="errors"></small></div>
<div class="card">UART activity<b id="bytes">—</b><small id="sync"></small></div>
<div class="card">Wi-Fi / MQTT<b id="network">—</b><small id="ip"></small></div>
<div class="card">Free memory<b id="heap">—</b><small id="uptime"></small></div>
<div class="card">Receiver status<b id="health">—</b><small id="reset"></small></div>
</section>
<section class="panel"><h2>Diagnostic log</h2>
<div class="controls"><label>Severity <select id="level"><option value="2" selected>Info, warnings &amp; errors</option><option value="3">Include trace</option><option value="1">Warnings &amp; errors</option><option value="0">Errors only</option></select></label><button id="clear">Clear view</button><a href="/api/v1/debug/download" target="_blank">Export log</a><a href="/api/v1/status" target="_blank">JSON status</a></div>
<pre id="logs" aria-live="polite">Waiting for gateway events...</pre>
</section>
<p class="muted">This page polls every 2 seconds. It does not control or transmit to Evohome. Keep the gateway on a trusted local network; do not expose HTTP port 80 to the Internet.</p>
<script>
const el=id=>document.getElementById(id);
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
set('health',s.radio_detected?(s.valid_frames?'Receiving':'Waiting for RF'):'Check radio');
set('reset','Last reset: '+(s.reset_reason||'unknown'));
const response=await fetch('/api/v1/debug/logs?since='+cursor+'&level='+el('level').value,{cache:'no-store'});
if(!response.ok)throw new Error('logs HTTP '+response.status);
const data=await response.json();
if(cursor===0 && lines.length===0)set('logs','');
for(const item of data.entries||[])add('['+Math.floor(item.uptime_ms/1000)+'s] '+item.level.padEnd(5)+' '+item.message);
cursor=Number(data.next||cursor);
}catch(error){set('connection','Disconnected');el('connection').className='bad';if(lines.length===0)set('logs','Network error: '+error.message)}
finally{working=false}
}
el('clear').addEventListener('click',()=>{lines=[];set('logs','');cursor=0;poll()});
el('level').addEventListener('change',()=>{lines=[];set('logs','');cursor=0;poll()});
poll();setInterval(poll,2000);
</script></body></html>)GWHTML";
