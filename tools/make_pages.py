#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the AMB-1 site for GitHub Pages (https://charlesvestal.github.io/fm1-amb/):

  index.html                  what AMB-1 is, and the ways in: try, install, manual, download, source
  img/                        screenshots (docs/img)
  emu/                        AMB-1 in the browser (build/emu, from web/emu/build.sh)
  install/index.html          the web installer (web/amb_installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/     the manual (tools/manual_html.py, from docs/MANUAL.md)
  firmware/amb-VERSION.fwsc   the package the installer writes; also the download

  tools/make_pages.py build/amb-0.9.fwsc 0.9 OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_7XXYYZZ, is what
the installer checks the download against and what the FM-1 reports after the install."""
import html
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

REPO = "https://github.com/charlesvestal/fm1-amb"

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AMB-1</title>
<meta name="description" content="A generative ambient machine for the M-VAVE FM-1, inspired by Brian Eno's Music for Airports: breathing voices, bells on tape loops of unequal length, a slow tide.">
<meta name="theme-color" content="#fdf9e4">
<style>
/* the colours of Ambient 1: Music for Airports (1978), as the manual: the page a shade lighter than the
   cover's paper, so the screenshots (on that paper) stand out; the cover's paper for the cards */
:root { --paper: #fdf9e4; --sheet: #f6eeb5; --green-1: #d0dc92; --green-3: #9cb16f; --river: #85c3c1; --river-2: #549995;
        --ink: #1d1b02; --muted: #4f4b25; --rule: #c2ce7f; color-scheme: light; }
body { background: var(--paper); color: var(--ink); margin: 0; padding-inline: 16px; padding-block: 48px 64px;
       font: 400 17px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif; }
main { max-width: 42rem; margin: 0 auto; }
h1 { font-size: 3rem; font-weight: 300; line-height: 1; letter-spacing: .06em; margin: 0 0 .3rem; }
.sub { text-transform: uppercase; letter-spacing: .08em; font-size: .9rem; margin: 0 0 1.2rem; padding-bottom: .8rem; border-bottom: 1px solid var(--ink); }
h2 { font-size: 1.1rem; font-weight: 400; text-transform: uppercase; letter-spacing: .06em; margin: 2.4rem 0 .6rem;
     padding-top: .6rem; border-top: 1px solid var(--ink); }
.lede { font-size: 1.15rem; margin: 0 0 1.6rem; max-width: 36rem; }
.shots { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin: 0 0 1.8rem; }
.shots img { width: 100%; height: auto; image-rendering: pixelated; border: 1px solid var(--ink); }
.ways { display: grid; gap: 10px; margin: 0 0 1rem; }
.ways a { display: block; text-decoration: none; color: var(--ink); background: var(--sheet); border: 1px solid var(--ink); padding: 12px 16px; }
.ways a:first-child { background: var(--river); }
.ways a:hover, .ways a:focus-visible { background: var(--green-1); outline: none; }
.ways strong { font-weight: 400; font-size: 1.15rem; text-transform: uppercase; letter-spacing: .05em; display: block; }
.ways span { color: var(--muted); font-size: .95rem; }
.ways em.v { font-style: normal; text-transform: none; letter-spacing: 0; }
ul.garden { padding-left: 1.2rem; margin: 0 0 1rem; }
ul.garden li { margin: 0 0 .4rem; }
ul.garden li::marker { color: var(--river-2); }
p.small { color: var(--muted); font-size: .9rem; }
a { color: var(--ink); text-decoration-color: var(--river-2); text-decoration-thickness: 2px; text-underline-offset: 3px; }
code { font-size: .9em; }
</style>
</head>
<body>
<main>
<h1>AMB-1</h1>
<p class="sub">An ambient garden for the M-VAVE FM-1</p>
<p class="lede">A generative ambient machine, inspired by Brian Eno's <i>Music for Airports</i>. It plays itself from the moment it's switched on. You don't perform it, you tend it: pick a
piece and a scale, take notes out, put them back, and let it grow.</p>
<div class="shots">
  <img src="img/screen-garden.png" width="240" height="240" alt="AMB-1's screen: the scale as a ring with the chord Dm7, the four bell loops around it, the pulse rings on the right">
  <img src="img/screen-voices.png" width="240" height="240" alt="AMB-1's screen: the piece Voices, F Aeolian, the chord Fm7">
  <img src="img/screen-frozen.png" width="240" height="240" alt="AMB-1's screen: frozen, the chord held">
</div>
<nav class="ways" aria-label="Get AMB-1">
  <a href="emu/"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Mouse, touch or keyboard; no FM-1 needed.</span></a>
  <a href="install/"><strong>Install <em class="v">v__VERSION__</em></strong><span>From Chrome or Edge, with the FM-1 connected by USB. Nothing to install on the computer.</span></a>
  <a href="manual/"><strong>Manual</strong><span>Getting started in a few minutes, then everything else.</span></a>
  <a href="firmware/__PKG__"><strong>Download <em class="v">__PKG__</em></strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
  <a href="__REPO__"><strong>Source</strong><span>GitHub, GPL-3.0. Built on Felucca by Hügelton Instruments.</span></a>
</nav>

<h2>The garden</h2>
<ul class="garden">
  <li><b>Pad.</b> Four breathing voices, a Rhodes or sung voices, each swelling and fading on its own cycle, so the chord forms and dissolves. Now and then it drifts to another chord in the scale.</li>
  <li><b>Bells.</b> Soft FM bells on four tape loops 13, 17, 23 and 29 beats long, sliding against each other so the melody never quite repeats.</li>
  <li><b>Bass.</b> The chord's root, low and gliding.</li>
  <li><b>Pulse.</b> A thump and two hats on Euclidean patterns of 16, 12 and 7 steps.</li>
  <li><b>Space.</b> A tape echo, a long tape loop and a plate reverb: most of what you hear. REC freezes the chord and lets the reverb ring on forever.</li>
</ul>
<p>Eight pieces, each remembering how you left it. The keys are the scale: press one to take its note out or
put it back. A slow tide between sparse and full, and rests now and then, keep it moving for hours.</p>
<p class="small">To go back to the official firmware, use the installer's "Back to the stock firmware", or
M-VAVE's updater, M-UPGRADE, from <a href="https://www.m-vave.com/download">m-vave.com/download</a>.
M-VAVE and FM-1 are trademarks of their owners. AMB-1 is not affiliated with M-VAVE or Brian Eno.</p>
</main>
</body>
</html>
"""


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_7\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"amb-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "firmware"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    # the installer
    page = (SRC / "web" / "amb_installer.html").read_text(encoding="utf-8")
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"amb_installer.html must contain {mark} once")
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    # the manual (a complete page; its images beside it)
    subprocess.run([sys.executable, str(SRC / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html")], check=True, capture_output=True)
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the landing page and its screenshots
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name)
                                    .replace("__REPO__", REPO), encoding="utf-8")
    emu = SRC / "build" / "emu"
    if not (emu / "amb.wasm").exists():
        raise SystemExit("no build/emu/amb.wasm: run web/emu/build.sh (needs Emscripten)")
    shutil.copytree(emu, out / "emu")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), manual/, emu/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
