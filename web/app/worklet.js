// SPDX-License-Identifier: GPL-3.0-only
// The AMB-1 web app's audio: amb.wasm (the engine alone, as on the FM-1) in an AudioWorklet. The page
// posts commands; each render quantum runs the engine for 128 frames; about 30 times a second, while
// the page is on screen, the garden's state goes back to it for drawing. Nothing is allocated per
// quantum (the views onto the engine's memory are kept), so no garbage collection on the audio thread;
// and nothing is sent while the page is hidden, so no messages pile up behind a throttled page.

class Amb extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.gain = 2400;
    this.last = 0;
    this.visible = true;
    this.mem = null;                   // the views onto the engine's output, kept while its memory stays
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  async onMessage(m) {
    if (m.t === "load") {
      const env = new Proxy({}, { get: () => () => 0 });
      const wasi = new Proxy({}, { get: () => () => 0 });
      const { instance } = await WebAssembly.instantiate(m.wasm, { env, wasi_snapshot_preview1: wasi });
      this.ex = instance.exports;
      if (this.ex._initialize) this.ex._initialize();
      this.ex.app_init();
      this.n = this.ex.app_state_len();
      for (const c of m.queue || []) this.apply(c);
      this.port.postMessage({ t: "ready" });
    } else if (this.ex) {
      this.apply(m);
    }
  }

  apply(m) {
    const ex = this.ex;
    switch (m.t) {
      case "set": ex.app_set(m.p, m.v); break;
      case "pool": ex.app_pool(m.v); break;
      case "play": ex.app_play(m.v ? 1 : 0); break;
      case "next": ex.app_next(); break;
      case "freeze": ex.app_freeze(m.v ? 1 : 0); break;
      case "seed": ex.app_seed(m.v | 0); break;
      case "regrow": ex.app_regrow(); break;
      case "gain": this.gain = m.v | 0; break;
      case "vis": this.visible = !!m.v; break;
    }
  }

  process(inputs, outputs) {
    const ex = this.ex;
    if (!ex) return true;
    const out = outputs[0], n = out[0].length;
    ex.app_render(n, this.gain);
    if (this.mem !== ex.memory.buffer || this.l.length !== n) {   // first, or the memory grew
      this.mem = ex.memory.buffer;
      this.l = new Float32Array(this.mem, ex.app_out_l(), n);
      this.r = new Float32Array(this.mem, ex.app_out_r(), n);
    }
    out[0].set(this.l);
    if (out[1]) out[1].set(this.r);
    if (this.visible && currentTime - this.last >= 1 / 30) {
      this.last = currentTime;
      const st = new Float32Array(ex.memory.buffer, ex.app_state(), this.n).slice();
      const pat = [ex.app_drum_pat(0), ex.app_drum_pat(1), ex.app_drum_pat(2)];
      this.port.postMessage({ t: "state", st, pat }, [st.buffer]);
    }
    return true;
  }
}

registerProcessor("amb-app", Amb);
