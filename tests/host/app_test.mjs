// SPDX-License-Identifier: GPL-3.0-only
// The web app's engine (build/app/amb.wasm) in Node: the pieces' recipes, the parameters' texts, a minute
// of a piece rendered (sound, no NaN, bounded), a piece change, the state the page draws.
import fs from "fs";
const bytes = fs.readFileSync(process.argv[2]);
const stub = new Proxy({}, { get: () => () => 0 });
const { instance } = await WebAssembly.instantiate(bytes, { env: stub, wasi_snapshot_preview1: stub });
const ex = instance.exports;
if (ex._initialize) ex._initialize();
const cstr = (p) => { const m = new Uint8Array(ex.memory.buffer); let e = p; while (m[e]) e++; return new TextDecoder().decode(m.subarray(p, e)); };
let fails = 0;
const check = (ok, what) => { if (!ok) { console.log("FAIL", what); fails++; } };
check(ex.app_npieces() === 8, "eight pieces");
check(cstr(ex.app_piece_name(2)) === "Voices", "piece 2 is Voices");
check(ex.app_piece_par(1, 18) === 60, "Departures' tempo 60");
check(cstr(ex.app_param_text(6, 100)) === "5:00", "Drift's text");
check(cstr(ex.app_chord_name(2, 2, 5, 9, 0)) === "Dm7", "a chord's name: " + cstr(ex.app_chord_name(2, 2, 5, 9, 0)));
ex.app_init();
for (let p = 0; p < ex.app_nparams(); p++) ex.app_set(p, ex.app_piece_par(0, p));
ex.app_pool(ex.app_piece_pool(0));
ex.app_play(1);
let peak = 0, bad = 0;
const run = (secs) => { for (let i = 0; i < secs * 44100 / 128; i++) { ex.app_render(128, 2400);
  for (const a of [ex.app_out_l(), ex.app_out_r()]) for (const v of new Float32Array(ex.memory.buffer, a, 128)) { if (v !== v) bad++; peak = Math.max(peak, Math.abs(v)); } } };
run(30);
check(!bad && peak > 0.05 && peak < 1, `a piece plays (peak ${peak.toFixed(3)})`);
const st = new Float32Array(ex.memory.buffer, ex.app_state(), ex.app_state_len());
check(st[0] === 1 && st[16] > 100, "the state: playing, the grid running");
for (let p = 0; p < ex.app_nparams(); p++) ex.app_set(p, ex.app_piece_par(2, p));   /* Voices */
ex.app_pool(ex.app_piece_pool(2));
ex.app_regrow();
peak = 0;
run(20);
check(!bad && peak > 0.02 && peak < 1, `another piece plays (peak ${peak.toFixed(3)})`);
console.log(fails ? `app: ${fails} FAILED` : "app: ok");
process.exit(fails ? 1 : 0);
