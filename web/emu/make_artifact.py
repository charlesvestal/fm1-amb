#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The emulator as a private claude.ai Artifact (the repo stays private, there is no public site):
build/emu (web/emu/build.sh) -> build/artifact/{index.html, worklet.js}. The page loses its
document wrapper (the Artifact host adds one) and its site links; amb.wasm goes into it as base64,
so nothing is fetched."""
import base64
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EMU, OUT = ROOT / "build/emu", ROOT / "build/artifact"
OUT.mkdir(parents=True, exist_ok=True)
s = (EMU / "index.html").read_text()
s = re.sub(r"<!doctype html>\s*<html[^>]*>\s*<head>\s*", "", s)
s = re.sub(r'<meta charset="utf-8">\s*<meta name="viewport"[^>]*>\s*', "", s)
s = s.replace("</head>\n<body>\n", "").replace("</body>\n</html>\n", "")
s = re.sub(r"\s*<nav>.*?</nav>", "", s)
wasm = base64.b64encode((EMU / "amb.wasm").read_bytes()).decode()
old = 'fetch("amb.wasm").then((r) => { if (!r.ok) throw new Error(`amb.wasm: ${r.status}`); return r.arrayBuffer(); })'
assert old in s
s = s.replace(old, "Promise.resolve(Uint8Array.from(atob(AMB_WASM), (c) => c.charCodeAt(0)).buffer)")
s = s.replace('<script>\n"use strict";', f'<script>\n"use strict";\nconst AMB_WASM = "{wasm}";', 1)
s = s.replace("<title>AMB-1 in the browser</title>", "<title>AMB-1 Emulator</title>", 1)
(OUT / "index.html").write_text(s)
(OUT / "worklet.js").write_text((EMU / "worklet.js").read_text())
print(f"artifact: {OUT / 'index.html'} ({len(s)} B), worklet.js")
