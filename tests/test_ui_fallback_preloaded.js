/* Run with Node.js; simulate a preloaded *old* Lovelace/HACS module which
 * registers evohome-dashboard-panel first. This must never hide the new HA
 * sidebar's heat-demand feature or backend version label.
 */
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const root = path.resolve(__dirname, "..");
const implementations = [
  "custom_components/evohome_dashboard/frontend/dashboard-legacy.js",
  "frontend-package/dist/evohome-dashboard-ui.js",
];
for (const relative of implementations) {
  class ElementStub {}
  class StalePanel extends ElementStub {}
  class StaleCard extends ElementStub {}
  const elements = new Map([
    ["evohome-dashboard-panel", StalePanel],
    ["evohome-dashboard-card", StaleCard],
  ]);
  const customElements = {
    get(name) { return elements.get(name); },
    define(name, ctor) {
      assert.ok(!elements.has(name), "Attempted to replace a preloaded element: " + name);
      assert.ok(![...elements.values()].includes(ctor), "Constructor already registered: " + name);
      elements.set(name, ctor);
    },
  };
  const source = fs.readFileSync(path.join(root, relative), "utf8");
  vm.runInNewContext(source, {
    HTMLElement: ElementStub,
    customElements,
    window: {},
  }, {filename:relative});
  assert.equal(elements.get("evohome-dashboard-panel"), StalePanel);
  assert.equal(elements.get("evohome-dashboard-card"), StaleCard);
  const updated = elements.get("evohome-dashboard-ha-panel");
  assert.ok(updated, relative + " failed to register the HA-owned panel");
  assert.notEqual(updated, StalePanel);
  assert.equal(typeof updated.prototype.heatDemand, "function");
  assert.ok(source.includes("this.backendVersion"), "Missing backend version label");
}
console.log("Preloaded stale custom elements: current HA-owned dashboard remains available");
