#pragma once
#include <pgmspace.h>
// Embedded standalone browser script, served separately from HTML to keep
// each ESP32 HTTP response small; full diagnostics are still available.
static const char GW_WEB_JS[] PROGMEM = R"GWJS(
const el=id=>document.getElementById(id);
const inFlight=new Set();
async function scheduled(name,fn){
 if(document.visibilityState==='hidden'||inFlight.has(name))return;
 inFlight.add(name);
 try{await fn()}finally{inFlight.delete(name)}
}

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
function refreshCapture(){if(el('packet-capture-panel').open)scheduled('capture',updatePacketCapture)}
el('packet-capture-panel').addEventListener('toggle',refreshCapture);
setInterval(refreshCapture,10000);

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
scheduled('rx-diagnostics',updateRxDiagnostics);
setInterval(()=>scheduled('rx-diagnostics',updateRxDiagnostics),15000);
async function updateRxHealth(){
 try{
  const response=await fetch('/api/v1/radio/rx-health',{cache:'no-store'});
  if(!response.ok)throw Error('HTTP '+response.status);
  set('rx-health',JSON.stringify(await response.json(),null,2));
 }catch(e){set('rx-health','RX health unavailable: '+e.message)}
}
function refreshRxHealth(){if(el('rx-health-panel').open)scheduled('rx-health',updateRxHealth)}
el('rx-health-panel').addEventListener('toggle',refreshRxHealth);
setInterval(refreshRxHealth,10000);

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
const readBlock={
 configuration_unavailable:'Gateway settings not loaded.',
 disable_automatic_opentherm_rf:'Turn off automatic OpenTherm TX, polling and scan in Settings.',
 configure_r8810a_id:'Configure the RF ID of your R8810A in Settings.',
 r8810a_not_observed:'Waiting to receive a valid RF frame from the configured R8810A.',
 opentherm_scan_active:'An OpenTherm scan is already running.',
 requires_868_mhz_module:'Requires an 868 MHz CC1101 board configured for 868300 kHz.',
 controller_identity_ambiguous:'More than one controller identity was seen. Reads blocked to avoid a source collision.',
 evohome_display_not_observed:'Waiting for an actual 01: Evohome Display RF transmission after boot.',
 evohome_bridge_source_conflict:'Evohome Display identity conflicts with bridge-linked evidence. Reads blocked.',
 controller_id_coverage_exhausted:'Controller-ID observation limit exceeded; reads blocked.',
 pairing_active:'An RF pairing transaction is underway.',
 another_read_active:'Another R8810A read is in progress.',
 one_shot_active:'A legacy ID25 RF test is in progress.',
 cv_batch_active:'A CV read session is already active.',
 read_cooldown:'Previous read is cooling down (60 seconds).',
 rf_quiet_window:'Waiting for RF quiet window (2.5 seconds).',
 radio_not_ready:'The CC1101 is not ready.'
};
const whyReadBlocked=code=>readBlock[code]||('Read blocked: '+code);
let bridgeReadNotice='';
async function updatePairedBridgeRead(){
 try{
  const r=await fetch('/api/v1/opentherm/paired-read',{cache:'no-store'});
  if(!r.ok)throw new Error('HTTP '+r.status);
  const v=await r.json();
  el('bridge-read-send').disabled=!v.ready||!el('bridge-read-confirm').checked;
  set('bridge-read-result',
    'Target R8810A: 10:'+String(v.bridge_id).padStart(6,'0')+
    ' · source existing Evohome: 01:'+String(v.controller_id).padStart(6,'0')+
    '\\nBridge heard: '+v.bridge_seen+' · direct controller request seen: '+v.direct_controller_observed+
    '\\nDisplay source 01:'+String(v.controller_id).padStart(6,'0')+' · ready: '+v.ready+
    ' · cooldown: '+Math.ceil(v.cooldown_ms/1000)+'s'+
    (v.block_reason?'\\nWaiting: '+whyReadBlocked(v.block_reason):'\\nReady for an authorized read.')+
    '\\nLast read ID '+v.data_id+' '+v.data_name+' · state: '+v.state+
    (v.has_value?' · '+Number(v.value).toFixed(2)+' (native units)':
      v.state==='read_ack'?' · raw 0x'+Number(v.raw).toString(16).toUpperCase().padStart(4,'0'):' · no verified Read-Ack')+
    '\\nLocal TX: '+v.tx_phase+' ('+v.tx_bytes+'/'+v.tx_expected_bytes+' bytes)'+
    '\\n'+(bridgeReadNotice||v.note));
 }catch(e){
  el('bridge-read-send').disabled=true;
  set('bridge-read-result','R8810A direct read unavailable: '+e.message);
 }
}
el('bridge-read-confirm').addEventListener('change',updatePairedBridgeRead);
el('bridge-read-send').addEventListener('click',async()=>{
 el('bridge-read-send').disabled=true;
 try{
  const id=el('bridge-read-id').value;
  const r=await fetch('/api/v1/opentherm/paired-read',{
   method:'POST',
   headers:{'X-Gateway-Config':'paired-bridge-read',
            'Content-Type':'application/x-www-form-urlencoded'},
   body:new URLSearchParams({
     data_id:id,
     confirm:'one-read-own-r8810a-as-observed-controller'
   }).toString()
  });
  const reply=await r.json();
  if(!r.ok)throw new Error(reply.error||'HTTP '+r.status);
  bridgeReadNotice='One READ-only RQ queued to R8810A; awaiting its response.';
 }catch(e){
  bridgeReadNotice='R8810A read blocked: '+e.message;
 }
 el('bridge-read-confirm').checked=false;
 await updatePairedBridgeRead();
});
function refreshPairedBridgeRead(){
 if(el('bridge-read-panel').open)scheduled('bridge-read',updatePairedBridgeRead)
}
el('bridge-read-panel').addEventListener('toggle',refreshPairedBridgeRead);
scheduled('bridge-read',updatePairedBridgeRead);
setInterval(refreshPairedBridgeRead,10000);

// CV snapshot reads are driven by the gateway radio task, not by browser timers.
// A refresh/reconnect resumes presentation without issuing duplicate RF requests.
const cvCatalog=[
 {id:18,name:'CH water pressure',unit:'bar',digits:2,group:'Heating circuit'},
 {id:25,name:'Boiler CH flow temperature',unit:'°C',digits:2,group:'Heating circuit'},
 {id:28,name:'Heating return (unverified)',unit:'°C',digits:2,group:'Heating circuit'},
 {id:57,name:'Maximum CH setpoint',unit:'°C',digits:2,group:'Heating circuit'},
 {id:117,name:'CH pump starts',unit:'starts',digits:0,group:'Heating circuit'},
 {id:121,name:'CH pump operating hours',unit:'hours',digits:0,group:'Heating circuit'},
 {id:17,name:'Burner modulation',unit:'%',digits:1,group:'Boiler operation'},
 {id:33,name:'Exhaust temperature',unit:'°C',digits:0,group:'Boiler operation'},
 {id:26,name:'Domestic hot-water temperature',unit:'°C',digits:2,group:'Boiler operation'},
 {id:34,name:'Heat-exchanger temperature',unit:'°C',digits:2,group:'Boiler operation'},
 {id:35,name:'Fan speed setpoint / actual',unit:'',digits:0,group:'Boiler operation'},
 {id:36,name:'Flame current',unit:'µA',digits:2,group:'Boiler operation'},
 {id:116,name:'Burner starts',unit:'starts',digits:0,group:'Boiler operation'},
 {id:120,name:'Burner operating hours',unit:'hours',digits:0,group:'Boiler operation'},
 {id:5,name:'Fault / service flags',unit:'',digits:0,group:'Service diagnostics'},
 {id:115,name:'Manufacturer diagnostic code',unit:'',digits:0,group:'Service diagnostics'}
];
let cvPosting=false,cvPostError='';
function cvFormatValue(field,result){
 if(!result||result.state!=='read_ack')return '—';
 const raw=Number(result.raw)||0;
 if(field.id===5||field.id===115)
  return '0x'+raw.toString(16).toUpperCase().padStart(4,'0');
 if(field.id===35)
  return 'Set '+(raw>>8)+' / actual '+(raw&255)+' (encoded)';
 if(result.value===null||!Number.isFinite(Number(result.value)))return 'Raw '+raw;
 return Number(result.value).toFixed(field.digits)+' '+field.unit;
}
function cvDraw(snapshot){
 const tbody=el('cv-read-results');
 tbody.replaceChildren();
 const passive=snapshot.passive_ch||{};
 const passiveRow=document.createElement('tr');
 const passiveData=[
  'ID 0 · Central heating active (passive)',
  passive.observed?(passive.ch_active?'CH active':'CH inactive'):'—',
  passive.observed?'Observed bridge Read-Ack; no request sent':'Awaiting natural bridge status; never requested',
  passive.observed?'+ '+Math.round(passive.updated_at_ms/1000)+'s uptime':'—'
 ];
 for(const value of passiveData){
  const cell=document.createElement('td');
  cell.textContent=value;passiveRow.appendChild(cell);
 }
 tbody.appendChild(passiveRow);
 let lastGroup='';
 for(const field of cvCatalog){
  if(field.group!==lastGroup){
   const heading=document.createElement('tr');
   const title=document.createElement('th');
   title.colSpan=4;title.scope='rowgroup';title.textContent=field.group;
   heading.appendChild(title);tbody.appendChild(heading);
   lastGroup=field.group;
  }
  const item=(snapshot.results||[]).find(x=>x.id===field.id);
  const phase=item?.state||'not_tested';
  const tx= item?.tx_phase==='local_complete' &&
    item.tx_expected_bytes>0 && item.tx_bytes===item.tx_expected_bytes;
  const status=phase==='read_ack'?
      (tx?'Read-Ack / local TX complete':'Read-Ack / TX unverified'):
      phase.replace(/_/g,' ');
  const timestamp=item?.updated_at_ms?'+ '+Math.round(item.updated_at_ms/1000)+'s uptime':'—';
  const row=document.createElement('tr');
  for(const content of ['ID '+field.id+' · '+field.name,
        cvFormatValue(field,item),status,timestamp]){
   const cell=document.createElement('td');
   cell.textContent=content;
   row.appendChild(cell);
  }
  tbody.appendChild(row);
 }
}
async function cvStatus(){
 try{
  const response=await fetch('/api/v1/opentherm/cv-batch',{cache:'no-store'});
  if(!response.ok)throw Error('HTTP '+response.status);
  const s=await response.json();
  cvDraw(s);
  const finished=(s.results||[]).filter(x=>
   ['read_ack','data_invalid','unknown_data_id','timeout','tx_failed'].includes(x.state)).length;
  const acks=(s.results||[]).filter(x=>x.state==='read_ack').length;
  const phase=s.active?
    'Reading '+finished+'/'+s.count+' completed · '+acks+
       ' Read-Acks · 15s minimum RF spacing':
    s.completed?(s.aborted?'Stopped (RF preflight unavailable)':'Completed')+
      ': '+finished+'/'+s.count+' requests · '+acks+' Read-Acks':
    'Not read yet; one click requests 16 diagnostic values (15s spacing).';
  const source=s.source_controller_id?' · source 01:'+String(s.source_controller_id).padStart(6,'0'):'';
  set('cv-read-status',(cvPostError?cvPostError+' · ':'')+phase+source+
    (s.block_reason&&!s.active?' · '+whyReadBlocked(s.block_reason):
      s.ready&&!el('cv-read-confirm').checked?' · Tick authorization to enable Read CV data.':''));
  el('cv-read-start').disabled=cvPosting||s.active||!s.ready||
    !el('cv-read-confirm').checked;
 }catch(error){
  set('cv-read-status','CV status unavailable: '+error.message);
  el('cv-read-start').disabled=true;
 }
}
el('cv-read-confirm').addEventListener('change',()=>{cvPostError='';scheduled('cv-status',cvStatus)});
el('cv-read-start').addEventListener('click',async()=>{
 if(cvPosting||!el('cv-read-confirm').checked)return;
 cvPosting=true;
 el('cv-read-start').disabled=true;
 try{
  const response=await fetch('/api/v1/opentherm/cv-batch',{
   method:'POST',
   headers:{'X-Gateway-Config':'cv-read-batch',
            'Content-Type':'application/x-www-form-urlencoded'},
   body:new URLSearchParams({
    confirm:'sixteen-read-only-requests-existing-evohome-identity'
   }).toString()
  });
  const answer=await response.json();
  if(!response.ok)throw Error(answer.error||'HTTP '+response.status);
  cvPostError='';
  el('cv-read-confirm').checked=false;
  set('cv-read-status','CV read session queued on ESP32');
 }catch(error){cvPostError='CV request rejected: '+error.message;set('cv-read-status',cvPostError)}
 finally{cvPosting=false;await cvStatus()}
});
scheduled('cv-status',cvStatus);
setInterval(()=>scheduled('cv-status',cvStatus),5000);

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
let controllerStopTarget=0;
async function updatePairing(){
 try{
  const r=await fetch('/api/v1/opentherm/pairing',{cache:'no-store'});
  if(!r.ok)throw new Error('HTTP '+r.status);
  const x=await r.json();
  controllerStopTarget=Number(x.virtual_controller_id)||0;
  el('controller-forget').disabled=!(controllerStopTarget>0 &&
     x.state!=='not_started' && el('controller-forget-confirm').checked);
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
    ' · ID25 result: '+(x.verification_result||'not_tested')+
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
  el('controller-forget').disabled=true;
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
function refreshPairing(){if(el('pairing-panel').open)scheduled('pairing',updatePairing)}
el('pairing-panel').addEventListener('toggle',refreshPairing);
setInterval(refreshPairing,10000);
el('controller-forget-confirm').addEventListener('change',updatePairing);
el('controller-forget').addEventListener('click',async()=>{
 el('controller-forget').disabled=true;
 if(!el('controller-forget-confirm').checked || !(controllerStopTarget>0))return;
 try{
  const response=await fetch('/api/v1/opentherm/pairing/forget',{
   method:'POST',
   headers:{'X-Gateway-Config':'virtual-controller-forget',
            'Content-Type':'application/x-www-form-urlencoded'},
   body:new URLSearchParams({
     virtual_controller_id:String(controllerStopTarget),
     confirm:'stop-and-forget-local-01'
   }).toString()
  });
  const d=await response.json();
  if(!response.ok)throw new Error(d.error||'HTTP '+response.status);
  pairingPostError='Virtual 01: local stop queued; physical R8810A binding is unchanged.';
 }catch(e){pairingPostError='Local controller stop rejected: '+e.message}
 el('controller-forget-confirm').checked=false;
 await updatePairing();
});

let thermostatPostError='';
let thermostatStopTarget=0;
async function updateThermostat(){
 try{
  const response=await fetch('/api/v1/thermostat',{cache:'no-store'});
  if(!response.ok)throw new Error('HTTP '+response.status);
  const p=await response.json();
  thermostatStopTarget=Number(p.thermostat_id)||0;
  el('thermostat-forget').disabled=!(thermostatStopTarget>0 &&
    p.state!=='not_started' && el('thermostat-forget-confirm').checked);
  const candidate=Number(el('thermostat-id').value.trim());
  const idValid=Number.isInteger(candidate)&&candidate>0&&candidate<262143;
  const canPair=Boolean(p.ready)&&idValid&&el('thermostat-confirm').checked;
  el('thermostat-pair').disabled=!canPair;
  el('thermostat-set-temp').disabled=p.state!=='controller_accepted_confirmation_sent';
  set('thermostat-status',
    'Virtual sensor: '+p.state+' · 12:'+String(p.thermostat_id).padStart(6,'0')+
    '\nExisting Evohome: 01:'+String(p.controller_id).padStart(6,'0')+
    ' · accepted: '+p.offer_accepted+' · confirmed locally: '+p.confirmation_sent+
    '\nAssigned zone: '+p.zone+' · cooldown '+Math.ceil(p.cooldown_ms/1000)+'s'+
    ' · observed 12: IDs: '+p.ids_seen+
    '\nSaved temperature: '+(p.temperature_fresh?(p.temp_centi/100).toFixed(2)+'°C':'missing or stale')+
    '\nRF update: '+(p.report_pending?
      'pending ('+Math.ceil(p.next_report_in_ms/1000)+'s minimum delay remaining)':
      p.last_report_attempted?(p.last_report_local_complete?'locally transmitted':'local TX failed'):
      'no temperature report yet')+
    ' · scheduled refresh: '+(p.temperature_fresh?Math.ceil(p.next_report_in_ms/1000)+'s':'off')+
    '\nLast RF sample: '+(p.last_report_attempted?(p.last_report_centi/100).toFixed(2)+'°C':'none')+
    '\nLast TX '+p.last_tx_action+': '+p.tx_phase+' ('+p.tx_bytes+'/'+p.tx_expected_bytes+' bytes)'+
    '\nMQTT set topic: '+p.mqtt_temperature_topic+
    '\n'+(thermostatPostError||p.note));
 }catch(e){
  el('thermostat-pair').disabled=true;
  el('thermostat-set-temp').disabled=true;
  el('thermostat-forget').disabled=true;
  set('thermostat-status','Thermostat status unavailable: '+e.message);
 }
}
el('thermostat-forget-confirm').addEventListener('change',updateThermostat);
el('thermostat-forget').addEventListener('click',async()=>{
 el('thermostat-forget').disabled=true;
 if(!el('thermostat-forget-confirm').checked || !(thermostatStopTarget>0))return;
 try{
  const response=await fetch('/api/v1/thermostat/forget',{
   method:'POST',
   headers:{'X-Gateway-Config':'thermostat-forget',
            'Content-Type':'application/x-www-form-urlencoded'},
   body:new URLSearchParams({
     thermostat_id:String(thermostatStopTarget),
     confirm:'stop-and-forget-local-12'
   }).toString()
  });
  const d=await response.json();
  if(!response.ok)throw new Error(d.error||'HTTP '+response.status);
  thermostatPostError='Virtual 12: local stop queued; Evohome sensor assignment is unchanged.';
 }catch(e){thermostatPostError='Local thermostat stop rejected: '+e.message}
 el('thermostat-forget-confirm').checked=false;
 await updateThermostat();
});
el('thermostat-confirm').addEventListener('change',updateThermostat);
el('thermostat-id').addEventListener('input',updateThermostat);
el('thermostat-pair').addEventListener('click',async()=>{
 el('thermostat-pair').disabled=true;
 try{
  const response=await fetch('/api/v1/thermostat/pairing',{
   method:'POST',
   headers:{'X-Gateway-Config':'thermostat-pairing','Content-Type':'application/x-www-form-urlencoded'},
   body:new URLSearchParams({
    thermostat_id:el('thermostat-id').value,
    confirm:'manual-12-thermostat-bind'
   }).toString()
  });
  const data=await response.json();
  if(!response.ok)throw new Error(data.error||'HTTP '+response.status);
  thermostatPostError='12: thermostat offer queued; watching for controller acceptance.';
 }catch(e){thermostatPostError='Pairing rejected: '+e.message}
 el('thermostat-confirm').checked=false;
 await updateThermostat();
});
el('thermostat-set-temp').addEventListener('click',async()=>{
 el('thermostat-set-temp').disabled=true;
 try{
  const rawTemperature=el('thermostat-temp').value.trim();
  if(rawTemperature==='')throw new Error('room temperature is required');
  const degrees=Number(rawTemperature);
  if(!Number.isFinite(degrees)||degrees<-40||degrees>85)throw new Error('temperature outside -40..85°C');
  const response=await fetch('/api/v1/thermostat/temperature',{
   method:'POST',
   headers:{'X-Gateway-Config':'thermostat-temperature','Content-Type':'application/x-www-form-urlencoded'},
   body:new URLSearchParams({
    temp_centi:String(Math.round(degrees*100)),
    confirm:'set-sensor-temperature'
   }).toString()
  });
  const result=await response.json();
  if(!response.ok)throw new Error(result.error||'HTTP '+response.status);
  thermostatPostError=result.report_pending?
   'Saved '+degrees.toFixed(2)+'°C; awaiting RF report (earliest '+Math.ceil(result.next_report_in_ms/1000)+'s).':
   'Saved '+degrees.toFixed(2)+'°C; identical to last locally transmitted sample. No extra RF needed.';
 }catch(e){thermostatPostError='Temperature rejected: '+e.message}
 await updateThermostat();
});
function refreshThermostat(){if(el('thermostat-panel').open)scheduled('thermostat',updateThermostat)}
el('thermostat-panel').addEventListener('toggle',refreshThermostat);
setInterval(refreshThermostat,10000);

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
function refreshOneShot(){if(el('one-shot-panel').open)scheduled('one-shot',updateOneShot)}
el('one-shot-panel').addEventListener('toggle',refreshOneShot);
setInterval(refreshOneShot,10000);
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
scheduled('status',poll);
setInterval(()=>scheduled('status',poll),5000);
scheduled('opentherm',otStatus);
setInterval(()=>scheduled('opentherm',otStatus),15000);
el('tab-settings').addEventListener('click',()=>scheduled('config',loadConfig));
if(location.hash==='#settings')scheduled('config',loadConfig);
)GWJS";
