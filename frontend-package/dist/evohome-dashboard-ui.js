/* Evohome Dashboard UI — standalone HACS dashboard module (MIT license). */
/* EVOHOME_UI_FEATURE: heating-demand-v1 */
/* EVOHOME_UI_FEATURE: backend-version-label-v1 */
/* EVOHOME_UI_FEATURE: cloud-room-rf-link-v1 */
const EVOHOME_UI_SOURCE = "HACS";
const esc = (v) => String(v ?? "").replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
const modes = {auto:"Auto",auto_with_eco:"Eco",away:"Away",day_off:"Day Off",custom:"Custom",heating_off:"Heating Off",auto_with_reset:"Auto Reset"};
const DAY_NAMES = ["monday","tuesday","wednesday","thursday","friday","saturday","sunday"];
const css = [
":host{display:block;min-height:100%;background:#0b1422;color:#e7edf5;font:15px/1.5 system-ui,-apple-system,sans-serif}",
"*{box-sizing:border-box}button,input,select{font:inherit}button{cursor:pointer}",
".frame{max-width:1500px;margin:auto;padding:26px 28px 70px}",
".top{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:26px;flex-wrap:wrap}",
".brand{display:flex;align-items:center;gap:14px}.logo{display:grid;place-items:center;width:50px;height:50px;background:linear-gradient(135deg,#50dec3,#338cc4);border-radius:17px;color:#092c36;font-size:27px;font-weight:900}",
"h1,h2,h3,p{margin:0}h1{font-size:26px;letter-spacing:-.8px}h2{font-size:21px;letter-spacing:-.35px}h3{font-size:16px}",
".muted{color:#9baabd}.caption{color:#99acbd;font-size:12px}.sub{margin:4px 0 20px;color:#a5b3c4}",
".status{display:flex;gap:9px;align-items:center;background:#132335;border:1px solid #294056;padding:8px 13px;border-radius:999px;font-size:13px}",
".dot{height:9px;width:9px;background:#48d3a9;border-radius:50%}.dot.off{background:#f1a155}",
".nav{display:flex;gap:6px;margin:0 0 28px;padding:6px;border-radius:16px;background:#121f30;overflow-x:auto;width:max-content;max-width:100%}",
".nav button{color:#a6b8ca;background:transparent;border:0;border-radius:11px;padding:10px 16px;white-space:nowrap}",
".nav .active{color:#071d29;background:#70dfcc;font-weight:700}",
".layout{display:grid;grid-template-columns:repeat(12,minmax(0,1fr));gap:16px}",
".card{background:#142439;border:1px solid #284058;border-radius:20px;padding:21px;box-shadow:0 12px 40px #0001}",
".zone{grid-column:span 3;min-height:192px;display:flex;flex-direction:column;gap:12px}",
".zone:hover{border-color:#438f96}.zonehead{display:flex;justify-content:space-between;gap:8px}",
".temp{font-size:37px;font-weight:700;letter-spacing:-2px}.temp small{font-size:19px;letter-spacing:0;color:#a4bbc9}",
".target{display:flex;justify-content:space-between;align-items:center;margin-top:auto}",
".pill{border:1px solid #416478;color:#8df3da;border-radius:999px;padding:4px 10px;font-size:12px}",
".error-pill{border-color:#ad7657;color:#ffc08c}",
".btn{border:1px solid #416077;background:#1d3550;color:#e7f4ff;border-radius:11px;padding:9px 14px;font-weight:600}",
".btn:hover{border-color:#64d7c0}.btn.primary{border:0;background:#70dfcc;color:#10303d}.btn.warn{border-color:#c4895c}",
".btn:disabled{opacity:.45;cursor:not-allowed}.small{font-size:12px;padding:7px 11px}",
".wide{grid-column:span 12}.half{grid-column:span 6}.third{grid-column:span 4}",
".section{display:flex;justify-content:space-between;align-items:center;gap:12px;margin:10px 0 15px;flex-wrap:wrap}",
".gridbuttons{display:flex;flex-wrap:wrap;gap:10px}.mode{min-width:115px;text-align:center}",
".mode.selected{background:#3a766f;border-color:#78e8d3}",
".setting{display:flex;justify-content:space-between;align-items:center;padding:16px 0;gap:14px;border-bottom:1px solid #2b4359}",
".setting:last-child{border-bottom:0}.setting .caption{max-width:650px}",
".warning{border:1px solid #8c6845;background:#392d25;color:#ffd7aa;padding:14px 17px;border-radius:13px;margin:15px 0}",
".notice{background:#163b3c;border:1px solid #377978;padding:13px 15px;border-radius:12px;margin:12px 0;color:#bdf9e6}",
".form{display:flex;gap:12px;flex-wrap:wrap;align-items:end}.field{display:grid;gap:5px;min-width:130px;flex:1}",
"label{font-size:13px;color:#bacbdc}input,select{width:100%;border:1px solid #496077;border-radius:10px;background:#091827;color:#fff;padding:10px;min-height:41px}",
".daynav{display:flex;gap:5px;flex-wrap:wrap;margin:12px 0 18px}.daynav button{border:1px solid #344c63;background:#1a3044;color:#c1d3e3;border-radius:9px;padding:9px 14px}",
".daynav button.selected{background:#62d6c0;color:#10322e}",
".point{display:grid;grid-template-columns:1fr 1fr auto;gap:10px;align-items:center;padding:12px 0;border-bottom:1px solid #2e4459}",
".event{display:flex;justify-content:space-between;align-items:center;gap:12px;border-bottom:1px solid #2b4359;padding:13px 2px;flex-wrap:wrap}",
".empty{padding:54px 16px;text-align:center;color:#afbdcd}.headline{font-size:14px;font-weight:700;color:#edfaf9;margin-bottom:5px}",
".overlay{position:fixed;inset:0;background:#020c16c9;display:grid;place-items:center;padding:15px;z-index:100}",
".modal{background:#192a40;border:1px solid #486176;border-radius:20px;padding:26px;width:min(460px,100%);box-shadow:0 24px 80px #0009}",
".modal h2{margin-bottom:9px}.modal .form{margin:18px 0}",
".steps{display:flex;align-items:center;gap:12px}.steps button{width:42px;height:42px;border-radius:12px}",
"@media(max-width:1050px){.zone{grid-column:span 4}.half{grid-column:span 12}}",
"@media(max-width:720px){.frame{padding:14px 12px 70px}.zone{grid-column:span 6}.third{grid-column:span 12}.top{align-items:flex-start}.card{padding:16px}.temp{font-size:30px}.status{font-size:11px}.point{grid-template-columns:1fr 1fr auto}}",
"@media(max-width:420px){.zone{grid-column:span 12}.nav button{padding:9px 12px}.frame{padding:13px 9px 60px}}"
].join("\n");

class EvohomeDashboardPanel extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({mode:"open"});
    this.tab = "overview"; this.entry = null; this.entries = [];
    this.state = null; this.online = false; this.errors = 0; this.rfDemand = null; this.backendVersion = null; this.pendingRfLinks = {};
    this.banner = ""; this.loading = false; this.modal = null;
    this.schedule = null; this.scheduleSystem = ""; this.scheduleZone = "";
    this.activeDay = "monday"; this.timer = null; this.refreshing = false;
    this.shadowRoot.addEventListener("click", e => this.onClick(e));
    this.shadowRoot.addEventListener("change", e => this.onChange(e));
  }
  set hass(value) {
    this._hass = value;
    if (this.isConnected && !this.entry && !this.loading) this.bootstrap();
  }
  get hass() { return this._hass; }
  connectedCallback() {
    this.draw();
    if (this._hass) this.bootstrap();
    this.timer = setInterval(() => this.entry && !this.refreshing && this.fetchStatus(),30000);
  }
  disconnectedCallback() { clearInterval(this.timer); }
  async ws(type, values={}) {
    if (!this._hass) throw Error("Home Assistant is not connected");
    return this._hass.callWS({type:"evohome_dashboard/"+type,...values, ...(type==="entries"?{}:{entry_id:this.entry})});
  }
  async bootstrap() {
    if (this.loading || this.entry) return;
    this.loading = true; this.draw();
    try {
      const res = await this.ws("entries");
      this.entries = res.entries || [];
      if (!this.entries.length) throw Error("No Evohome Dashboard integration configured. Add it under Settings → Devices & services.");
      this.entry = this.entries[0].entry_id;
      await this.fetchStatus();
    } catch(e) { this.banner = e.message || String(e); }
    finally { this.loading = false; this.draw(); }
  }
  async fetchStatus() {
    if (!this.entry) return;
    this.refreshing=true;
    try {
      const x = await this.ws("status");
      this.state=x.state; this.online=x.online; this.errors=x.errors; this.rfDemand=x.heat_demand||null; this.backendVersion=x.backend_version||null; this.banner="";
    } catch(e) { this.banner = e.message || "Cannot load Evohome data"; }
    finally { this.refreshing=false;this.draw(); }
  }
  systems() {
    return (this.state?.locations || []).flatMap(l => (l.systems||[]).map(s=>({...s,location_name:l.name})));
  }
  zones() { return this.systems().flatMap(s=>(s.zones||[]).map(z=>({...z,system_id:s.id}))); }
  findZone(system,zone) { return this.zones().find(z=>z.system_id===system && z.id===zone); }
  async command(type,data,notice="Request completed") {
    this.loading=true;this.draw();
    try { await this.ws(type,data); this.banner=""; await this.fetchStatus(); this.banner=notice; return true; }
    catch(e) { this.banner="Action failed: "+(e.message||"See Error Log"); return false; }
    finally { this.loading=false;this.modal=null;this.draw(); }
  }
  renderZones(zones) {
    if (!zones.length) return '<div class="card wide empty">No heating zones returned by Resideo.</div>';
    return zones.map(z=>{
      const warning=(z.faults||[]).length > 0;
      return '<div class="card zone"><div class="zonehead"><div><h3>'+esc(z.name)+'</h3><span class="caption">'+esc(z.kind||"Heating zone")+'</span></div><span class="pill '+(warning?"error-pill":"")+'">'+esc(warning?"Attention":z.mode==="follow_schedule"?"Scheduled":"Override")+'</span></div>'+
       '<div class="temp">'+esc(z.current==null?"—":Number(z.current).toFixed(1))+'<small> °C</small></div>'+
       '<div class="target"><span class="muted">Target <b style="color:#e9faf7">'+esc(z.target==null?"—":Number(z.target).toFixed(1))+'°</b></span>'+
      '<div class="gridbuttons"><button class="btn small" data-action="edit-zone" data-system="'+esc(z.system_id)+'" data-zone="'+esc(z.id)+'">Adjust</button>'+
      '<button class="btn small" data-action="zone-schedule" data-system="'+esc(z.system_id)+'" data-zone="'+esc(z.id)+'">Schedule</button></div></div></div>';
    }).join("");
  }
  nav() {
    const items=[["overview","Overview"],["rooms","Rooms"],["demand","Heating demand"],["schedules","Schedules"],["actions","Quick actions"],["settings","Settings"],["errors","Error log"]];
    return '<nav class="nav" aria-label="Evohome navigation">'+items.map(a=>'<button data-action="tab" data-tab="'+a[0]+'" class="'+(this.tab===a[0]?"active":"")+'">'+a[1]+'</button>').join("")+'</nav>';
  }
  overview() {
    const systems=this.systems(),zones=this.zones();
    const faults=zones.reduce((n,z)=>n+(z.faults||[]).length,0);
    return '<div class="section"><div><h2>Home overview</h2><p class="muted">Every room at a glance. Adjust only what you need.</p></div><button class="btn" data-action="refresh">↻ Refresh</button></div>'+
       '<div class="layout"><div class="card third"><div class="caption">Heating zones</div><div class="temp">'+zones.length+'</div><div class="muted">Across '+systems.length+' system(s)</div></div>'+
       '<div class="card third"><div class="caption">System mode</div><div style="font-size:25px;font-weight:700;margin:15px 0">'+esc(systems[0]?modes[systems[0].mode]||systems[0].mode:"—")+'</div><button class="btn small" data-action="tab" data-tab="actions">Change mode</button></div>'+
       '<div class="card third"><div class="caption">Attention needed</div><div class="temp">'+faults+'</div><div class="muted">Reported device faults</div></div>'+
       '<div class="wide section"><h2>Rooms</h2><button class="btn small" data-action="tab" data-tab="rooms">See all</button></div>'+
       this.renderZones(zones)+'</div>';
  }
  heatDemand() {
    const data=this.rfDemand;
    if (!data?.enabled) {
      return '<h2>Heating demand</h2><div class="card wide"><p>Passive RF heat-demand monitoring is not configured.</p>'+
        '<p class="muted" style="margin-top:12px">Configure the RF controller ID (01:xxxxxx) and MQTT gateway topic in the Evohome integration options. Once RF packets arrive, link rooms directly here using the cloud room dropdowns—no JSON needed.</p>'+
        '<p class="caption">Cloud room temperatures alone cannot establish radiator or boiler demand.</p></div>';
    }
    const zones=data.zones||[];
    const cloudRooms=this.zones();
    const roomNameCounts={};
    cloudRooms.forEach(r=>{const key=r.name?.toLocaleLowerCase()||"";roomNameCounts[key]=(roomNameCounts[key]||0)+1;});
    const systemLocations={};
    this.systems().forEach(s=>{systemLocations[s.id]=s.location_name||"Location unknown";});

    const time=v=>v?new Date(v).toLocaleString():"Not observed";
    const shown=v=>v==null?"Unknown":Number(v).toFixed(1)+"%";
    const status=data.mqtt_connected?"Listening for RF packets":"MQTT unavailable — verify broker and reload integration";
    const cards=zones.map(z=>{
      const state=z.state==="unknown_stale"?"Stale / unknown":z.state==="requesting"?"Requesting heat":"Idle";
      const names=(z.devices||[]).map(d=>esc(d.id)+" ("+shown(d.demand_pct)+")").join(", ");
      const cloudRef=z.mapping||{};
      const savedValue=cloudRef.system_id&&cloudRef.zone_id ? cloudRef.system_id+"|"+cloudRef.zone_id : "";
      const currentValue=Object.prototype.hasOwnProperty.call(this.pendingRfLinks||{},z.index)
        ? this.pendingRfLinks[z.index] : savedValue;
      const roomOptions=cloudRooms.map(room=>{
        const key=room.system_id+"|"+room.id;
        const duplicate=(roomNameCounts[room.name?.toLocaleLowerCase()||""]||0)>1;
        const label=duplicate
          ? room.name+" — "+(systemLocations[room.system_id]||"Unknown location")+
            " / system "+room.system_id+" / zone "+room.id
          : room.name;
        return '<option value="'+esc(key)+'" '+(key===currentValue?"selected":"")+'>'+esc(label)+'</option>';
      }).join("");
      const linkHint=cloudRef.source==="rf_0004"?"Auto-matched using a controller RF zone-name report":
        cloudRef.source==="selected"?"Linked to an existing Evohome cloud room":
        cloudRef.source==="ambiguous"?"Duplicate cloud room name; select the correct room":
        cloudRef.source==="missing_cloud_zone"?"Previously linked room is no longer available":
        cloudRef.source==="manual"?"Using previous manual name mapping":"Not linked to a cloud room";
      const roomPicker='<div class="field" style="margin-top:12px"><label>Cloud room</label>'+
        '<select data-rf-index="'+esc(z.index)+'" aria-label="Cloud room for RF zone '+esc(z.index)+'">'+
        '<option value="">— No link —</option>'+roomOptions+'</select></div>'+
        '<button class="btn small" data-action="link-rf-zone" data-index="'+esc(z.index)+'" style="margin-top:10px">Save room link</button>'+
        '<p class="caption">'+esc(linkHint)+(z.rf_reported_name?" · Controller name: "+esc(z.rf_reported_name):"")+'</p>';

      return '<section class="card third"><div class="zonehead"><h3>'+esc(z.name)+'</h3><span class="pill">'+esc(z.index)+'</span></div>'+
        '<div class="temp">'+esc(shown(z.demand_pct))+'</div>'+
        '<p class="muted">'+esc(state)+'</p>'+
        '<div class="setting"><span>Estimated request time today</span><b>'+esc(z.today_minutes)+' min</b></div>'+
        '<div class="setting"><span>Demand-weighted time</span><b>'+esc(z.today_weighted_minutes)+' min</b></div>'+
        '<p class="caption">Last RF report: '+esc(time(z.last_seen))+'</p>'+
        '<p class="caption">Actuators: '+(names||"No RF packets yet")+'</p>'+
        roomPicker+
        (!z.mapped?'<p class="caption">Room name not mapped — RF index only.</p>':"")+
        '</section>';
    }).join("");
    const recent=(data.recent||[]).slice(0,20).map(event=>{
      const label=event.kind==="zone_demand"?"Zone "+esc(event.zone)+" / "+esc(event.source):"Controller 0008";
      return '<div class="event"><span>'+esc(time(event.time))+' — '+label+'</span><b>'+esc(shown(event.demand_pct))+'</b></div>';
    }).join("");
    return '<div class="section"><div><h2>Heating demand</h2><p class="muted">Passive RAMSES-II observations. Not measured gas consumption or proof of boiler firing.</p></div></div>'+
      '<div class="layout"><section class="card third"><h3>RF monitor</h3><p class="muted">'+esc(status)+'</p><p class="caption">Controller '+esc(data.controller_id)+'</p>'+
      '<p class="caption">Received '+esc(data.received)+' relevant/valid RF packet candidates</p></section>'+
      '<section class="card third"><h3>Controller 0008 demand</h3><div class="temp">'+esc(shown(data.controller_0008_pct))+'</div>'+
      '<p class="caption">Last seen: '+esc(time(data.controller_0008_last_seen))+'</p><p class="caption">Control signal, not actual gas use.</p></section>'+
      '<section class="card third"><h3>R8810A traffic</h3><p>'+esc(time(data.bridge_last_seen))+'</p>'+
      '<p class="caption">'+esc(data.bridge_frames)+' observed 3220/3EF0 exchanges; traffic does not prove a CH burner-on event.</p></section>'+
      '<div class="wide section"><h2>Rooms</h2><span class="caption">Demand messages expire after 25 minutes without a new packet.</span></div>'+
      (cards||'<div class="card wide empty">No valve demand packets received yet. Check CC1101 coverage and RF/controller settings.</div>')+
      '<section class="card wide"><h3>Recent demand reports</h3>'+
      (recent||'<p class="muted">No reports recorded yet.</p>')+'</section>'+
      '<p class="caption wide">Duration uses sample-and-hold between received packets, is capped by freshness and excludes unknown gaps. Per-room gas m³ cannot be measured from these messages.</p></div>';
  }
  rooms() {
    return '<div class="section"><div><h2>Room control</h2><p class="muted">Measured temperature and your target, with clear overrides.</p></div></div>'+
      '<div class="layout">'+this.renderZones(this.zones())+'</div>';
  }
  actions() {
    return '<h2>Quick actions</h2><p class="sub">Change the Evohome operating mode. Available modes depend on your controller.</p>'+
      '<div class="layout">'+this.systems().map(s=>'<section class="card half"><h3>'+esc(s.location_name)+'</h3><p class="sub">Current: '+esc(modes[s.mode]||s.mode)+'</p><div class="gridbuttons">'+
        (s.allowed_modes||[]).map(m=>'<button class="btn mode '+(m===s.mode?"selected":"")+'" data-action="set-mode" data-system="'+esc(s.id)+'" data-mode="'+esc(m)+'">'+esc(modes[m]||m)+'</button>').join("")+
       '</div><p class="caption" style="margin-top:15px">Use Auto to resume the controller schedule.</p></section>').join("")+
      this.systems().filter(s=>s.hot_water).map(s=>'<section class="card half"><h3>Hot water</h3><p class="sub">'+esc(s.location_name)+' — '+esc(s.hot_water.state)+'</p><div class="gridbuttons">'+
      '<button class="btn" data-action="hotwater" data-system="'+esc(s.id)+'" data-state="on">Turn on (1h)</button>'+
      '<button class="btn" data-action="hotwater" data-system="'+esc(s.id)+'" data-state="off">Turn off (1h)</button>'+
      '<button class="btn" data-action="hotwater-resume" data-system="'+esc(s.id)+'">Resume schedule</button></div></section>').join("")+'</div>';
  }
  schedules() {
    const zones=this.zones();
    const choose='<div class="field"><label for="schedule-zone">Heating zone</label><select id="schedule-zone">'+
       '<option value="">Select a room...</option>'+zones.map(z=>'<option value="'+esc(z.system_id)+'|'+esc(z.id)+'" '+(this.scheduleSystem===z.system_id&&this.scheduleZone===z.id?"selected":"")+'>'+esc(z.name)+'</option>').join("")+'</select></div>';
    let editor='';
    if (this.schedule) {
      const limits=this.schedule.limits||{};
      const day=this.schedule.schedule.find(x=>x.day_of_week===this.activeDay);
      editor='<div class="daynav">'+DAY_NAMES.map(n=>'<button class="'+(this.activeDay===n?"selected":"")+'" data-action="day" data-day="'+n+'">'+n.slice(0,3).toUpperCase()+'</button>').join("")+'</div>'+
        '<p class="caption">Changes remain local until you select Save. '+esc(limits.max_switchpoints_per_day||6)+' switchpoints maximum per day.</p>'+
        '<div>'+((day?.switchpoints)||[]).map((p,i)=>'<div class="point"><div class="field"><label>Time</label><input aria-label="Switchpoint time" type="time" step="600" data-point="'+i+'" data-field="time_of_day" value="'+esc(String(p.time_of_day).slice(0,5))+'"></div>'+
        '<div class="field"><label>Temperature °C</label><input aria-label="Switchpoint temperature" type="number" min="5" max="35" step="'+esc(limits.setpoint_value_resolution||0.5)+'" data-point="'+i+'" data-field="heat_setpoint" value="'+esc(p.heat_setpoint)+'"></div>'+
        '<button class="btn warn" title="Remove switchpoint" data-action="remove-point" data-point="'+i+'">×</button></div>').join("")+'</div>'+
        '<div class="section"><div class="gridbuttons"><button class="btn" data-action="add-point">+ Add switchpoint</button><button class="btn" data-action="copy-day">Copy this day</button></div><button class="btn primary" data-action="save-schedule">Save week</button></div>';
    } else { editor='<div class="empty">Select a room to view its controller schedule.</div>'; }
    return '<h2>Heating schedules</h2><p class="sub">Edit the schedule stored in Evohome itself, not an unrelated HA automation.</p>'+
       '<section class="card"><div class="form">'+choose+'</div>'+editor+'</section>';
  }
  settings() {
    const sys=this.systems()[0];
    const settings=[
      ["Installer / hidden settings","RF device binding, installation wizard, actuator testing","Controller-only — not exposed by verified cloud API"],
      ["Advanced heating settings","Optimum Start/Stop, weather compensation, boiler parameters","Not verified as remotely controllable"],
      ["Controller hardware settings","Display, language, Wi-Fi, firmware, RF link diagnostics","Physical Evohome controller required"],
      ["Destructive maintenance","Unbind devices, resets, factory defaults","Intentionally unavailable"],
    ];
    return '<h2>Settings & capabilities</h2><p class="sub">Transparent controls: we never pretend an undocumented device setting was changed.</p>'+
      '<section class="card"><h3>Cloud connection</h3><div class="setting"><div><strong>Controller</strong><div class="caption">'+esc(sys?.model||"Unknown")+'</div></div><span class="pill">'+esc(this.online?"Connected":"Unavailable")+'</span></div>'+
      '<div class="setting"><div><strong>Refresh interval</strong><div class="caption">Edit Settings → Devices & services → Evohome Dashboard → Configure</div></div><button class="btn small" data-action="refresh">Refresh now</button></div>'+
      '<div class="setting"><div><strong>Transport</strong><div class="caption">Resideo TCC cloud. A local RF/LAN connection has not been verified.</div></div><span class="pill">Cloud</span></div></section>'+
      '<h2 style="margin:26px 0 13px">Advanced / hidden Evohome settings</h2>'+
      '<div class="warning">The Evohome touchscreen has installer-level controls. This API does not expose most of them. These entries are informational only; no experimental RF commands or undocumented writes are sent.</div>'+
      '<h2 style="margin:26px 0 13px">Zone settings and capabilities</h2>'+
      '<section class="card">'+this.zones().map(z=>'<div class="setting"><div><strong>'+esc(z.name)+'</strong><p class="caption">Setpoint range '+esc(z.minimum)+'–'+esc(z.maximum)+' °C · Step '+esc(z.step)+' °C · Mode '+esc(z.mode)+'</p>'+
      '<p class="caption">Override until: '+esc(z.until||"Schedule / permanent")+' · '+esc((z.faults||[]).length)+' active fault(s)</p></div>'+
      '<button class="btn small" data-action="zone-schedule" data-system="'+esc(z.system_id)+'" data-zone="'+esc(z.id)+'">Edit schedule</button></div>').join("")+'</section>'+
      '<section class="card">'+settings.map(s=>'<div class="setting"><div><strong>'+esc(s[0])+'</strong><p class="caption">'+esc(s[1])+'</p></div><span class="caption">'+esc(s[2])+'</span></div>').join("")+'</section>';
  }
  async loadErrors() {
    try { const r=await this.ws("errors"); this.errorEvents=r.events||[];this.draw(); }
    catch(e) { this.banner=e.message||"Error log unavailable";this.draw(); }
  }
  errorPage() {
    const events=this.errorEvents||[];
    return '<div class="section"><div><h2>Error log</h2><p class="muted">Last '+events.length+' recorded incidents. Credentials and request contents are never logged.</p></div><button class="btn" data-action="load-errors">↻ Reload</button></div>'+
      '<section class="card">'+(events.length?events.map(e=>'<div class="event"><div><div class="headline">'+esc(e.operation)+'</div><div class="caption">'+esc(e.time)+' · '+esc(e.target||"General")+'</div></div><span class="pill error-pill">'+esc(e.category)+'</span></div>').join(""):'<div class="empty">No recorded errors.</div>')+'</section>';
  }
  renderModal() {
    if(!this.modal) return '';
    const z=this.findZone(this.modal.system,this.modal.zone);
    if(!z)return '';
    return '<div class="overlay"><section class="modal" role="dialog" aria-modal="true" aria-label="Adjust heating"><h2>'+esc(z.name)+'</h2><p class="muted">Current '+esc(z.current)+'°C · Setpoint '+esc(z.target)+'°C</p>'+
      '<div class="form"><div class="field"><label for="setpoint">Target temperature</label><input id="setpoint" type="number" min="'+esc(z.minimum)+'" max="'+esc(z.maximum)+'" step="'+esc(z.step||0.5)+'" value="'+esc(z.target)+'"></div>'+
      '<div class="field"><label for="duration">Override duration</label><select id="duration"><option value="1">1 hour</option><option value="2">2 hours</option><option value="4">4 hours</option><option value="24">24 hours</option><option value="permanent">Until manually cancelled</option></select></div></div>'+
      '<div class="gridbuttons"><button class="btn primary" data-action="confirm-zone">Apply temperature</button><button class="btn warn" data-action="zone-frost">Frost protection (min °C)</button><button class="btn" data-action="resume-zone">Resume schedule</button><button class="btn" data-action="close-modal">Cancel</button></div>'+
      '<div class="caption" style="margin-top:12px">Frost protection uses the zone minimum ('+esc(z.minimum)+'°C); it is not a physical RF shutoff.</div>'+
      '<p class="caption" style="margin-top:18px">Cloud writes may take several seconds to reach the controller.</p></section></div>';
  }
  draw() {
    const root=this.shadowRoot;
    const body=this.tab==="overview"?this.overview():this.tab==="rooms"?this.rooms():this.tab==="demand"?this.heatDemand():this.tab==="schedules"?this.schedules():this.tab==="actions"?this.actions():this.tab==="settings"?this.settings():this.errorPage();
    root.innerHTML='<style>'+css+'</style><div class="frame"><header class="top"><div class="brand"><div class="logo">⌂</div><div><h1>Evohome</h1><div class="caption">Home heating, made clear · HA backend '+esc(this.backendVersion?"v"+this.backendVersion:"version unknown")+' · UI: '+EVOHOME_UI_SOURCE+'</div></div></div>'+
      '<div class="status"><span class="dot '+(this.online?"":"off")+'"></span>'+esc(this.online?"Cloud connected":"Cloud not confirmed")+
      ' · '+esc(this.state?.updated?new Date(this.state.updated).toLocaleTimeString():"Awaiting data")+'</div></header>'+this.nav()+
      (this.banner?'<div class="'+(this.banner.startsWith("Action failed")?"warning":"notice")+'" role="alert">'+esc(this.banner)+'</div>':"")+
      (this.loading?'<div class="caption" aria-live="polite">Working…</div>':"")+
      (!this.state?'<div class="card empty">'+(this.loading?"Connecting to Evohome…":"Waiting for heating data. Check your integration setup.")+'</div>':body)+
      '<footer style="margin-top:36px" class="caption">Evohome Dashboard · Phase 1 · Cloud dependent · Installer-only controls are not remotely exposed</footer></div>'+this.renderModal();
  }
  async onClick(event) {
    const node=event.target.closest("[data-action]");if(!node || this.loading)return;
    const action=node.dataset.action;
    if(action==="tab"){this.tab=node.dataset.tab;this.draw();if(this.tab==="errors")this.loadErrors();return;}
    if(action==="link-rf-zone"){
      const index=node.dataset.index;
      const selector=this.shadowRoot.querySelector('select[data-rf-index="'+index+'"]');
      const selected=Object.prototype.hasOwnProperty.call(this.pendingRfLinks||{},index)
        ? this.pendingRfLinks[index] : (selector?.value||"");
      const [system_id,zone_id]=selected?selected.split("|"):["",""];
      const success=await this.command("rf_zone_link",{index,system_id,zone_id},
        selected?"RF room linked to Evohome cloud zone":"RF room link removed");
      if(success){
        delete this.pendingRfLinks[index];
        this.draw();
      }
      return;
    }

    if(action==="refresh"){await this.command("refresh",{},"Cloud data refreshed");return;}
    if(action==="load-errors"){this.loadErrors();return;}
    if(action==="edit-zone"){this.modal={system:node.dataset.system,zone:node.dataset.zone};this.draw();return;}
    if(action==="zone-schedule"){
      this.tab="schedules";
      this.scheduleSystem=node.dataset.system;this.scheduleZone=node.dataset.zone;
      this.modal=null;this.schedule=null;this.draw();
      try{this.schedule=await this.ws("schedule_get",{system_id:this.scheduleSystem,zone_id:this.scheduleZone});this.banner="";}
      catch(e){this.banner="Schedule unavailable: "+(e.message||"See Error log");}
      this.draw();return;
    }
    if(action==="zone-frost"){
      const z=this.findZone(this.modal.system,this.modal.zone);
      if(z && confirm("Set "+z.name+" to "+z.minimum+"°C frost protection?")){
        await this.command("zone_frost",{system_id:this.modal.system,zone_id:this.modal.zone},"Zone set to frost protection");
      }
      return;
    }
    if(action==="close-modal"){this.modal=null;this.draw();return;}
    if(action==="confirm-zone"){
      const temp=Number(this.shadowRoot.querySelector("#setpoint").value);
      const d=this.shadowRoot.querySelector("#duration").value;
      if(!Number.isFinite(temp)){this.banner="Enter a valid target temperature";this.draw();return;}
      const data={system_id:this.modal.system,zone_id:this.modal.zone,temperature:temp};
      if(d!=="permanent")data.until=new Date(Date.now()+Number(d)*3600000).toISOString();
      await this.command("zone_set",data,"Heating target updated");return;
    }
    if(action==="resume-zone"){await this.command("zone_resume",{system_id:this.modal.system,zone_id:this.modal.zone},"Room back on schedule");return;}
    if(action==="set-mode"){
      await this.command("system_mode",{system_id:node.dataset.system,mode:node.dataset.mode},"System mode updated");return;
    }
    if(action==="hotwater"){
      await this.command("hotwater_set",{system_id:node.dataset.system,state:node.dataset.state,until:new Date(Date.now()+3600000).toISOString()},"Hot water updated");return;
    }
    if(action==="hotwater-resume"){await this.command("hotwater_resume",{system_id:node.dataset.system},"Hot water follows schedule");return;}
    if(action==="day"){this.activeDay=node.dataset.day;this.draw();return;}
    if(action==="add-point"){const day=this.schedule?.schedule.find(x=>x.day_of_week===this.activeDay);if(!day)return;
      const max=this.schedule.limits?.max_switchpoints_per_day||6;
      if(day.switchpoints.length>=max){this.banner="Maximum switchpoints reached";this.draw();return;}
      const p=day.switchpoints[day.switchpoints.length-1];
      let hour=p?Number(p.time_of_day.slice(0,2))+1:7;hour=Math.min(hour,23);
      day.switchpoints.push({time_of_day:String(hour).padStart(2,"0")+":00:00",heat_setpoint:p?p.heat_setpoint:20});
      day.switchpoints.sort((a,b)=>a.time_of_day.localeCompare(b.time_of_day));this.draw();return;
    }
    if(action==="remove-point"){
      const day=this.schedule?.schedule.find(x=>x.day_of_week===this.activeDay);if(!day)return;
      if(day.switchpoints.length<=(this.schedule.limits?.min_switchpoints_per_day||1)){
        this.banner="At least one switchpoint required";this.draw();return;
      }
      day.switchpoints.splice(Number(node.dataset.point),1);this.draw();return;
    }
    if(action==="copy-day"){
      const target=prompt("Copy this day to (e.g. tuesday):");
      if(!target)return;
      const dayname=target.toLowerCase().trim();
      if(!DAY_NAMES.includes(dayname)){this.banner="Invalid day";this.draw();return;}
      const source=this.schedule.schedule.find(d=>d.day_of_week===this.activeDay);
      this.schedule.schedule.find(d=>d.day_of_week===dayname).switchpoints=structuredClone(source.switchpoints);
      this.banner="Copied to "+dayname+"; remember to Save";this.draw();return;
    }
    if(action==="save-schedule"){
      if(!confirm("Write this full weekly schedule to the Evohome controller?"))return;
      await this.command("schedule_set",{
        system_id:this.scheduleSystem,zone_id:this.scheduleZone,
        schedule:this.schedule.schedule,expected_version:this.schedule.version,
      },"Weekly schedule saved and verified");this.schedule=null;this.draw();return;
    }
  }
  async onChange(event) {
    const node=event.target;
    if(node.dataset.rfIndex!=null){
      this.pendingRfLinks[node.dataset.rfIndex]=node.value;
      return;
    }
    if(node.id==="schedule-zone"){
      const [system,zone]=node.value.split("|");this.scheduleSystem=system||"";this.scheduleZone=zone||"";
      this.schedule=null;this.draw();
      if(!system||!zone)return;
      try {
        this.schedule=await this.ws("schedule_get",{system_id:system,zone_id:zone});
        this.banner="";this.activeDay="monday";
      } catch(e){this.banner="Schedule load failed: "+(e.message||"See error log");}
      this.draw();return;
    }
    if(node.dataset.point!=null && node.dataset.field && this.schedule){
      const day=this.schedule.schedule.find(d=>d.day_of_week===this.activeDay);
      const point=day?.switchpoints[Number(node.dataset.point)];if(!point)return;
      point[node.dataset.field]=node.dataset.field==="heat_setpoint"?Number(node.value):node.value+":00";
    }
  }
}
if(!customElements.get("evohome-dashboard-panel")) {
  customElements.define("evohome-dashboard-panel", EvohomeDashboardPanel);
}
// The HA sidebar owns this separate element name. Old HACS resources may
// already have registered evohome-dashboard-panel in the same browser page;
// a distinct subclass and tag allow the current backend UI to render anyway.
class EvohomeDashboardHAPanel extends EvohomeDashboardPanel {}
if(!customElements.get("evohome-dashboard-ha-panel")) {
  customElements.define("evohome-dashboard-ha-panel", EvohomeDashboardHAPanel);
}

/* Lovelace embedding: add /hacsfiles/evohome-dashboard-ui/evohome-dashboard-ui.js as a module
 * resource; then create a manual card with type: custom:evohome-dashboard-card.
 * Both panel and card use the exact same zone and schedule logic. */
class EvohomeDashboardCard extends HTMLElement {
  constructor() {
    super();
    this.panel=document.createElement("evohome-dashboard-panel");
    this.append(this.panel);
  }
  set hass(value) { this.panel.hass = value; }
  setConfig(config) {
    if(config && config.type && config.type !== "custom:evohome-dashboard-card") {
      throw new Error("Incorrect card type");
    }
  }
  getCardSize() { return 6; }
}
if(!customElements.get("evohome-dashboard-card")) {
  customElements.define("evohome-dashboard-card", EvohomeDashboardCard);
}
window.customCards = window.customCards || [];
if(!window.customCards.some(x=>x.type==="evohome-dashboard-card")) {
  window.customCards.push({type:"evohome-dashboard-card",name:"Evohome Dashboard",description:"Heating zones, schedules, modes and diagnostics"});
}
