#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The web app as a private claude.ai Artifact: build/app (web/app/build.sh) -> build/app-artifact/
{index.html, worklet.js}. The page loses its document wrapper (the Artifact host adds one); amb.wasm
goes into it as base64, so nothing is fetched. build/app itself is the static site (charles.pizza/amb, web/app/publish.sh)."""
import base64
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
APP, OUT = ROOT / "build/app", ROOT / "build/app-artifact"
OUT.mkdir(parents=True, exist_ok=True)
s = (APP / "index.html").read_text()
s = re.sub(r"<!doctype html>\s*<html[^>]*>\s*<head>\s*", "", s)
s = re.sub(r'<meta (charset|name="viewport"|name="apple-mobile-web-app-capable"|name="mobile-web-app-capable"|name="theme-color")[^>]*>\s*', "", s)
s = s.replace("</head>\n<body>\n", "").replace("</body>\n</html>\n", "")
wasm = base64.b64encode((APP / "amb.wasm").read_bytes()).decode()
s = s.replace('<script>\n"use strict";', f'<script>\n"use strict";\nconst AMB_WASM = "{wasm}";', 1)
(OUT / "index.html").write_text(s)
(OUT / "worklet.js").write_text((APP / "worklet.js").read_text())
print(f"app artifact: {OUT / 'index.html'} ({len(s)} B), worklet.js")
