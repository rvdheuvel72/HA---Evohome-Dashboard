/* Verify both shipped dashboards render and invoke the R8810A test UI. */
"use strict";
const assert=require("node:assert/strict");
const fs=require("node:fs");
const path=require("node:path");
const vm=require("node:vm");
const root=path.resolve(__dirname,"..");
(async()=>{
  for(const relative of [
    "custom_components/evohome_dashboard/frontend/dashboard-legacy.js",
    "frontend-package/dist/evohome-dashboard-ui.js",
  ]){
    const elements=new Map();
    const context={
      HTMLElement:class{},
      customElements:{
        get(k){return elements.get(k);},
        define(k,v){elements.set(k,v);}
      },
      window:{},
      confirm:()=>true
    };
    vm.runInNewContext(fs.readFileSync(path.join(root,relative),"utf8"),context,{filename:relative});
    const Panel=elements.get("evohome-dashboard-ha-panel");
    assert.ok(Panel,relative+" must register the sidebar dashboard");
    const panel=Object.create(Panel.prototype);
    panel.diagAuthorized=false;
    panel.diagReplaceCapture=false;
    panel.loading=false;
    panel.diag={configured:true,state:"idle",running:false,reads:[],packet_count:0};
    panel.draw=()=>{};
    const html=panel.diagnosticPage();
    assert.ok(html.includes("Test ID25"));
    assert.ok(html.includes("Run IDs 18, 25, 117"));
    assert.ok(html.includes("data-action=\"diag-existing\""));
    assert.ok(html.includes("data-action=\"diag-export-json\""));
    assert.ok(html.includes("data-action=\"diag-export-capture\""));
    assert.ok(html.includes("disabled"),"RF initiation must start disabled");

    let request;
    panel.diagAuthorized=true;
    panel.ws=async(name,body)=>{
      request={name,body};
      return {configured:true,running:true,state:"running",reads:[],stage:"Waiting"};
    };
    await panel.startDiagnostic(25);
    assert.equal(request.name,"rf_diag_start");
    assert.equal(request.body.data_id,25);
    assert.equal(request.body.replace_capture,false);
    assert.equal(request.body.confirm,"authorize-three-rf-reads-observed-evohome-controller-identity");
    assert.equal(panel.diagAuthorized,false,"authorization must reset after request");
    const running=panel.diagnosticPage();
    assert.ok(running.includes("Running: Waiting"));

    panel.diag={configured:true,state:"complete",running:false,reads:[{
      id:25,label:"Boiler flow temperature",state:"read_ack",
      value:55.25,tx_phase:"local_complete",tx_bytes:44,tx_expected_bytes:44,
      rf_reply_candidates:[],
    }],packet_count:0};
    const completed=panel.diagnosticPage();
    assert.ok(completed.includes("55.25"));
    assert.ok(completed.includes("read_ack"));
    assert.ok(completed.includes("Download report.json"));
    assert.ok(completed.includes("Download capture.tsv"));

    const saved=[];
    panel.downloadText=(name,data)=>saved.push({name,data});
    panel.ws=async()=>({report:{state:"complete",reads:[{id:25,state:"read_ack"}]},capture_tsv:"packet\n"});
    await panel.downloadDiagnostic("json");
    assert.equal(saved.length,1,"One click produces one download");
    assert.equal(JSON.parse(saved[0].data).reads[0].id,25);
    await panel.downloadDiagnostic("capture");
    assert.equal(saved.length,2);
    assert.ok(saved[1].name.endsWith(".tsv"));

    panel.diag={configured:false};
    assert.ok(panel.diagnosticPage().includes("One-time gateway configuration"));
    assert.ok(panel.nav().includes("R8810A test"));
  }
  console.log("Both dashboards: R8810A page, auth gated action and result display PASS");
})().catch(error=>{console.error(error);process.exitCode=1;});
