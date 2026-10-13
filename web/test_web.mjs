// SPDX-License-Identifier: GPL-3.0-only
// SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro)
//
// Node checks of the web installer library (no browser, no hardware). Run from the repo root:
//   node web/test_web.mjs
// - fm1ota.js: installs, resumes and refusals against a simulated FM-1 / SMK-37
// - fm1pkg.js: package identities longer than 20 characters

import { blockCount, logicalImage, productOf } from "./fm1pkg.js";
import { Updater, pack7, unpack7, sameKeyboard, OUR_LOADER, OTHER_SMK } from "./fm1ota.js";
let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(64)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };
const eq = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
/* ------------------------------------------------- update protocol (fm1ota.js) --- */
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];

/* an FM-1 on WebMIDI: identity, then "device asks, host answers" reads of the image */
class FakeFM1 {
  constructor(image, { unplugAfter = Infinity, finalIdentity = "FM-1_900", identity = "FM-1_015", name = "FM-1",
                      loader = "ota-FM-1_900", outName = (n) => n, readAll = false, jump = false } = {}) {
    this.jump = jump;   // the running firmware restarts into its loader on UPGRADE, without step 1
    this.image = image; this.unplugAfter = unplugAfter; this.served = 0; this.bad = 0;
    this.readAll = readAll; this.written = new Uint8Array(image.length); this.wrote = 0;   // readAll: the loader reads it all
    this.finalIdentity = finalIdentity; this.loader = loader; this.outName = outName;
    this.access = { inputs: new Map(), outputs: new Map() };
    this.boot(identity, name);
  }
  boot(identity, name) {
    this.identity = identity; this.waiting = null; this.queue = [];
    for (const m of [this.access.inputs, this.access.outputs]) { for (const p of m.values()) p.state = "disconnected"; m.clear(); }
    const id = Math.random().toString(36).slice(2);
    this.input = { id: "i" + id, name, state: "connected", onmidimessage: null, open: async () => {} };
    this.output = { id: "o" + id, name: this.outName(name), state: "connected", open: async () => {}, send: (d) => {
      if (this.output.state !== "connected") throw new Error("InvalidStateError");
      setTimeout(() => this.rx(Array.from(d)), 1);
    } };
    this.access.inputs.set(this.input.id, this.input);
    this.access.outputs.set(this.output.id, this.output);
  }
  tx(bytes) { const i = this.input; setTimeout(() => { if (i.state === "connected" && i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 1); }
  rx(d) {
    if (eq(d, HS)) {
      const t = [...new TextEncoder().encode(this.identity)];
      const body = [0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)];
      this.tx([0xF0, ...pack7(body), 0xF7]);
    } else if (eq(d, UPGRADE)) {
      if (this.jump && !this.identity.startsWith("ota-")) { this.input.state = this.output.state = "disconnected"; setTimeout(() => this.boot(this.loader, "Felucca Update"), 300); return; }
      const n = this.readAll ? Math.ceil(this.image.length / 512) : 6;
      this.queue = this.identity.startsWith("ota-")
        ? [...Array.from({ length: n }, (_, k) => [k * 512, Math.min(512, this.image.length - k * 512)]), [0xF0000000, 8]]
        : [[0, 64], [0x40, 160], [0x1000, 512], [0xE0000000, 8]];
      this.next();
    } else if (this.waiting) {
      const u = unpack7(d.slice(1, -1));
      const [addr, len] = this.waiting;
      const got = u.slice(14, 14 + (addr >= 0xE0000000 ? 8 : len));
      const want = addr >= 0xE0000000 ? [...new TextEncoder().encode("success"), 0] : Array.from(this.image.subarray(addr, addr + len));
      if (!eq(got, want)) this.bad++;
      if (addr < 0xE0000000 && this.identity.startsWith("ota-")) { this.written.set(got, addr); this.wrote += got.length; }
      this.waiting = null;
      this.served++;
      if (this.served >= this.unplugAfter) { this.input.state = this.output.state = "disconnected"; return; }
      if (addr === 0xE0000000) setTimeout(() => this.boot(this.loader, "Felucca Update"), 300);
      else if (addr === 0xF0000000) setTimeout(() => this.boot(this.finalIdentity, "Felucca"), 300);
      else this.next();
    }
  }
  next() {
    const r = this.queue.shift();
    if (!r) return;
    this.waiting = r;
    const [addr, len] = r;
    const u = [0, 0x59, 0x30, 0, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, len >> 8, 0];
    let s = 0;
    for (let i = 6; i < 14; i++) s += u[i];
    u.push(~s & 0xFF);
    this.tx([0xF0, ...pack7(u), 0xF7]);
  }
}

async function updater() {
  const image = Uint8Array.from({ length: 0x2000 }, (_, i) => (i * 7) & 0xFF);
  const dev = new FakeFM1(image);
  const steps = [];
  const got = await new Updater(dev.access).install(image, "FM-1_900", (k) => steps.push(k));
  ok(got === "FM-1_900" && dev.bad === 0 && steps.includes("write") && steps.at(-1) === "done",
    `fm1ota.js: install: running firmware -> loader -> Felucca (${dev.served} reads)`);

  const rescue = new FakeFM1(image);
  rescue.boot("FM-1_000", "Felucca");
  const recovered = await new Updater(rescue.access).install(image, "FM-1_900");
  ok(recovered === "FM-1_900" && rescue.bad === 0, "fm1ota.js: recovery mode -> loader -> normal firmware");
  const failedBoot = new FakeFM1(image, { finalIdentity: "FM-1_000" });
  const rescueSteps = [];
  const bootError = await new Updater(failedBoot.access).install(image, "FM-1_900", (k) => rescueSteps.push(k)).then(() => null, (e) => e);
  ok(bootError?.code === "mismatch" && !rescueSteps.includes("done"), "fm1ota.js: boot into recovery is not reported as successful installation");

  const dev2 = new FakeFM1(image, { unplugAfter: 3 });
  dev2.boot("ota-FM-1_900", "Felucca Update");
  const t0 = Date.now();
  const done = await new Updater(dev2.access).resume(image, null, { package: "FM-1_900" });
  ok(done === false && Date.now() - t0 < 6000, "fm1ota.js: unplugged during the write -> stops at once");

  const dev3 = new FakeFM1(image, { unplugAfter: 2 });
  const e = await new Updater(dev3.access).install(image, "FM-1_900").then(() => null, (x) => x);
  ok(e && e.code === "lost", "fm1ota.js: unplugged in step 1 -> error code 'lost'");
  const e2 = await new Updater({ inputs: new Map(), outputs: new Map() }).install(image, "FM-1_900").then(() => null, (x) => x);
  ok(e2 && e2.code === "notfound", "fm1ota.js: no device -> error code 'notfound'");
  {   /* resume() checks the loader's model against the package, by install()'s rule */
    const smkOta = new FakeFM1(image);
    smkOta.boot("ota-SMK-37 Pro_900", "Felucca Update");
    const m1 = await new Updater(smkOta.access).resume(image, null, { package: "FM-1_900" }).then(() => null, (x) => x);
    ok(m1 && m1.code === "model" && smkOta.served === 0, "fm1ota.js: an SMK-37 Pro in update mode is never given an FM-1 package ('model')");
    const noPkg = new FakeFM1(image);
    noPkg.boot("ota-FM-1_900", "Felucca Update");
    const m2 = await new Updater(noPkg.access).resume(image).then(() => null, (x) => x);
    ok(m2 && m2.code === "model" && noPkg.served === 0, "fm1ota.js: resume without the package's identity writes nothing");
    const elite = new FakeFM1(image);
    elite.boot("ota-SMK-37 Elite_900", "Felucca Update");
    const m3 = await new Updater(elite.access).resume(image, null, { package: "SMK-37 Pro_900" }).then(() => null, (x) => x);
    ok(m3 && m3.code === "model" && elite.served === 0, "fm1ota.js: resume: another SMK model only with anyModel, as install()");
    const elite2 = new FakeFM1(image);
    elite2.boot("ota-SMK-37 Elite_900", "Felucca Update");
    const r6 = await new Updater(elite2.access).resume(image, null, { package: "SMK-37 Pro_900", anyModel: true });
    ok(r6 === true && elite2.served > 0, "fm1ota.js: resume with anyModel: an SMK-37 Pro package on an Elite's loader is written");
  }
  const stock = new FakeFM1(image);
  stock.boot("ota-FM-1_015", "FM-1 Update");
  const e3 = await new Updater(stock.access).resume(image).then(() => null, (x) => x);
  ok(e3 && e3.code === "foreign" && e3.detail === "ota-FM-1_015" && stock.served === 0, "fm1ota.js: another firmware's loader is never resumed ('foreign')");
  const back = new FakeFM1(image, { finalIdentity: "FM-1_015" });   /* the return to the official V15, interrupted */
  back.boot("ota-FM-1_015", "FM-1 Update");
  const st = [];
  const r4 = await new Updater(back.access).resume(image, (k) => st.push(k), { product: "FM-1_015" });
  ok(r4 === true && back.bad === 0 && st.at(-1) === "done", "fm1ota.js: return to official: its own loader resumed, V15 checked when back");
  const wrong = new FakeFM1(image, { finalIdentity: "FM-1_900" });
  wrong.boot("ota-FM-1_015", "FM-1 Update");
  const e5 = await new Updater(wrong.access).resume(image, null, { product: "FM-1_015" }).then(() => null, (x) => x);
  ok(e5 && e5.code === "mismatch", "fm1ota.js: return to official: another firmware coming back is not 'done'");
  /* the SMK-37 models: one keyboard under several names */
  ok(sameKeyboard("SMK-37 Elite", "SMK-37 Pro") && sameKeyboard("ota-MKE-P37", "SMK-37 Elite") && sameKeyboard("STARRYKEY-37 PLAY", "SMK-37 Pro")
    && sameKeyboard("SMK-37+VA", "MKE-P37") && !sameKeyboard("FM-1", "SMK-37 Pro") && !sameKeyboard("SMK-37 Elite", "FM-1"),
    "fm1ota.js: SMK-37 Pro / Elite / MKE-P37 / Starrykey / +VA are one keyboard, the FM-1 another");
  ok(OUR_LOADER({ text: "ota-SMK-37 Elite_900" }) && OUR_LOADER({ text: "ota-MKE-P37_700" }) && !OUR_LOADER({ text: "ota-SMK-37 Elite_016" }),
    "fm1ota.js: our loader under the Elite / MKE-P37 name is ours, the stock ones are not");
  const smk = { finalIdentity: "SMK-37 Pro_900", loader: "ota-SMK-37 Pro_900" };
  const elite = new FakeFM1(image, { ...smk, identity: "SMK-37 Elite_016", name: "SMK-37 Elite" });
  const e7 = await new Updater(elite.access).install(image, "SMK-37 Pro_900").then(() => null, (x) => x);
  ok(e7 && e7.code === "model" && elite.served === 0, "fm1ota.js: an Elite and a Pro package: refused without anyModel");
  const elite2 = new FakeFM1(image, { ...smk, identity: "SMK-37 Elite_016", name: "SMK-37 Elite" });
  ok(await new Updater(elite2.access).install(image, "SMK-37 Pro_900", null, true) === "SMK-37 Pro_900" && elite2.bad === 0,
    "fm1ota.js: ... installed with anyModel (the same keyboard)");
  const fm1 = new FakeFM1(image);
  const e8 = await new Updater(fm1.access).install(image, "SMK-37 Pro_900", null, true).then(() => null, (x) => x);
  ok(e8 && e8.code === "model" && fm1.served === 0, "fm1ota.js: an SMK package never goes to an FM-1, anyModel or not");
  {   /* ChanceTheMaker's SMK firmwares ("SMK-37_931"): their own updater, never sent ours */
    ok(!OUR_LOADER({ text: "ota-SMK-37_931" }) && OTHER_SMK({ text: "ota-SMK-37_931" }) && OTHER_SMK({ text: "SMK-37_941" }) && !OTHER_SMK({ text: "SMK-37 Pro_900" }),
      "fm1ota.js: the bare SMK-37_9NN firmwares and their updater are not ours");
    const other = new FakeFM1(image, { identity: "SMK-37_931", name: "Felucca", loader: "ota-SMK-37_931", jump: true });
    const r = await new Updater(other.access).install(image, "SMK-37 Pro_900", null, true).catch((x) => x);
    ok(r && r.code === "other" && other.served === 0 && other.identity === "SMK-37_931", "fm1ota.js: install: such a firmware is not even sent UPGRADE ('other')");
    const stuck = new FakeFM1(image, { finalIdentity: "SMK-37 Pro_900" });
    stuck.boot("ota-SMK-37_931", "Felucca Update");
    const r2 = await new Updater(stuck.access).resume(image, null, { package: "SMK-37 Pro_900", anyModel: true }).catch((x) => x);
    ok(r2 && r2.code === "foreign" && stuck.served === 0, "fm1ota.js: resume: its updater is never written ('foreign')");
    const jumper = new FakeFM1(image, { identity: "SMK-37 Pro_016", name: "SMK-37 Pro", loader: "ota-SMK-37 Pro_016", jump: true });
    const e9 = await new Updater(jumper.access).install(image, "SMK-37 Pro_900").catch((x) => x);
    ok(e9 && e9.code === "foreign" && jumper.served === 0, "fm1ota.js: a firmware that jumps to another loader: nothing written ('foreign')");
    const ours = new FakeFM1(image, { identity: "SMK-37 Pro_700", name: "Makkuro37", loader: "ota-SMK-37 Pro_700", finalIdentity: "SMK-37 Pro_900", jump: true });
    ok(await new Updater(ours.access).install(image, "SMK-37 Pro_900", null, true) === "SMK-37 Pro_900" && ours.bad === 0,
      "fm1ota.js: a firmware that jumps to one of our loaders: written through it");
  }
  const odd = new FakeFM1(image, { identity: "SMK-37 Pro_900", name: "USB Audio Device" });
  const log = [];
  ok(await new Updater(odd.access, (t) => log.push(t)).find() === null && log.some((t) => t.includes("USB Audio Device")),
    "fm1ota.js: a port of another name is not probed, its name is logged");
  const dodd = await new Updater(odd.access, null, { all: true }).find();
  ok(dodd && dodd.id.text === "SMK-37 Pro_900", "fm1ota.js: ... found when every port is searched");
  if (dodd) dodd.link.close();
  const win = new FakeFM1(image, { identity: "SMK-37 Pro_900", name: "MIDIIN2 (Felucca)", outName: (n) => n.replace("MIDIIN", "MIDIOUT") });
  win.access.outputs.set("other", { id: "other", name: "Microsoft GS Wavetable Synth", state: "connected", open: async () => {}, send: () => {} });
  const dwin = await new Updater(win.access).find();
  ok(dwin && dwin.id.text === "SMK-37 Pro_900", "fm1ota.js: MIDIIN2 (X) pairs with MIDIOUT2 (X)");
  if (dwin) dwin.link.close();
  const pk = await import("./fm1pkg.js");
  const notStock = new Uint8Array(pk.STOCK_V15_SIZE);
  const e6 = await pk.validateStockPackage(notStock).then(() => null, (x) => x);
  ok(e6 && /official FM-1 V15/.test(e6.message), "fm1pkg.js: only the exact official V15 is accepted (SHA-256)");
}

/* a package identity longer than 20 characters (the stock Starrykey 37 Play V016: 21 identity blocks, then 0x7D) */
function longIdentity() {
  const synth = (product, blocks) => {
    const img = Uint8Array.from({ length: 0x3000 }, (_, i) => (i * 7) & 0xFF), out = [];
    for (let i = 0; i < blocks; i++) {
      out.push(...img.subarray(i * 0x2F, (i + 1) * 0x2F), i < product.length ? (product.charCodeAt(i) + i + 1) & 0xFF : 0x7D);
    }
    out.push(...img.subarray(blocks * 0x2F));
    return Uint8Array.from(out);
  };
  const raw = synth("STARRYKEY-37 PLAY_016", 36);
  ok(blockCount(raw) === 36 && productOf(raw) === "STARRYKEY-37 PLAY_016" && logicalImage(raw).length === raw.length - 36,
    "fm1pkg.js: a 21-character identity in a 36-block package is read in full");
  const raw20 = synth("STARRYKEY-37 PLAY_015", 20);
  ok(blockCount(raw20) === 20 && productOf(raw20) === "STARRYKEY-37 PLAY_01", "fm1pkg.js: a 20-block package holds 20 characters");
}

await updater();
longIdentity();
console.log(failed ? `WEB TESTS FAILED (${failed})` : "web tests passed");
process.exit(failed ? 1 : 0);
