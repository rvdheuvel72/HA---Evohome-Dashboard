/* Run real dashboard UI methods in Node vm: cloud rooms populate RF selectors. */
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const root = path.resolve(__dirname, "..");

(async () => {
  for (const relative of [
    "custom_components/evohome_dashboard/frontend/dashboard-legacy.js",
    "frontend-package/dist/evohome-dashboard-ui.js",
  ]) {
    const elements = new Map();
    const context = {
      HTMLElement: class {},
      window: {},
      customElements: {
        get(key) { return elements.get(key); },
        define(key, value) { elements.set(key, value); },
      },
    };
    vm.runInNewContext(
      fs.readFileSync(path.join(root, relative), "utf8"), context,
      {filename: relative},
    );
    const Panel = elements.get("evohome-dashboard-ha-panel");
    assert.ok(Panel, relative + " must register the HA dashboard");
    const panel = Object.create(Panel.prototype);
    panel.loading = false;
    panel.pendingRfLinks = {};
    panel.draw = () => {};
    panel.tab = "demand";
    panel.state = {
      locations: [{systems: [{id:"sys1",zones:[
        {id:"cloud-100",name:"Woonkamer"},
        {id:"cloud-200",name:"Darius"},
        {id:"cloud-300",name:"Benjamin"},
        {id:"cloud-400",name:"Kantoor"},
        {id:"cloud-500",name:"Woonkamer"},
      ]}]}],
    };
    panel.rfDemand = {
      enabled:true,
      mqtt_connected:true,
      zones:[{
        index:"00",name:"RF zone 00",demand_pct:47,state:"requesting",
        mapped:false,today_minutes:22,today_weighted_minutes:12,
        last_seen:null,devices:[{id:"04:215289",demand_pct:47}],
        mapping:{source:"unlinked"},
      }],
      recent:[],controller_id:"01:245914",received:3,bridge_frames:0,
    };
    const html = panel.heatDemand();
    for (const name of ["Darius","Benjamin","Kantoor"]) {
      assert.ok(html.includes(">"+name+"</option>"), "Cloud room missing: "+name);
    }
    assert.ok(html.includes("Woonkamer — "), "Duplicate room name must be disambiguated");
    assert.ok(html.includes("zone cloud-100"), "Cloud zone ID must distinguish duplicates");
    assert.ok(html.includes("zone cloud-500"), "Other zone ID must be visible");
    assert.ok(html.includes('data-action="link-rf-zone"'));
    assert.ok(html.includes('data-rf-index="00"'));
    assert.ok(html.includes('value="sys1|cloud-100"'));
    assert.ok(html.includes("04:215289"));
    assert.ok(!html.includes("RF room mapping must be"), "Should not require JSON");
    // A background status redraw or saving a different card must not erase
    // this chosen (but unsaved) cloud room.
    await panel.onChange({
      target:{dataset:{rfIndex:"00"},value:"sys1|cloud-100"},
    });
    assert.equal(panel.pendingRfLinks["00"], "sys1|cloud-100");
    const redrawn = panel.heatDemand();
    assert.ok(redrawn.includes('value="sys1|cloud-100" selected'));


    let sent;
    panel.shadowRoot = {
      querySelector(selector) {
        assert.equal(selector, 'select[data-rf-index="00"]');
        return {value:"sys1|cloud-100"};
      },
    };
    panel.command = async (type, data) => { sent = {type, data}; return true; };
    await panel.onClick({
      target:{closest(){return {dataset:{action:"link-rf-zone",index:"00"}};}},
    });
    assert.equal(sent.type, "rf_zone_link");
    assert.equal(sent.data.index, "00");
    assert.equal(sent.data.system_id, "sys1");
    assert.equal(sent.data.zone_id, "cloud-100");
    assert.ok(!("00" in panel.pendingRfLinks), "Successful save should clear pending value");
  }
  console.log("Cloud room names appear in UI; selected stable zone IDs are sent to HA");
})().catch(err => {console.error(err);process.exitCode=1;});
