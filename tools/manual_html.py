#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""docs/MANUAL.md -> docs/manual.html. The Markdown is the source; run this after editing it.

  tools/manual_html.py [IN.md [OUT.html]] [--fragment]

--fragment writes the page without its <!doctype>/<html>/<head>/<body> wrapper (for hosts that
add their own); the default is a complete document that opens in any browser.

Handles what the manual uses: headings, paragraphs, **bold**, `code`, lists, tables, fenced
code, rules, and the contents list (linked to the sections by number)."""
import base64
import html
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
PART_COLOURS = {}                                     # (X0X marks its parts with a swatch; AMB-1 has none)

CSS = """
/* the colours of Ambient 1: Music for Airports (1978), as the web app: the map's paper, its greens,
   its rivers, the ink. Light only, as the cover is. The page a shade lighter than the cover's paper, so
   the screenshots (on that paper) stand out from it; the cover's paper for code and callouts. */
:root {
  --paper: #fdf9e4; --sheet: #f6eeb5; --green-1: #d0dc92; --green-3: #9cb16f;
  --river: #85c3c1; --river-2: #549995; --ink: #1d1b02; --muted: #4f4b25; --rule: #c2ce7f;
  color-scheme: light;
}
body {
  background: var(--paper); color: var(--ink); margin: 0;
  font: 400 17px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif;
  padding-inline: 16px; padding-block: 32px 64px;
}
main { max-width: 46rem; margin: 0 auto; }
h1, h2, h3 { font-family: inherit; text-wrap: balance; line-height: 1.2; }
h1 { font-size: 2.2rem; font-weight: 300; margin: 0 0 .4rem; letter-spacing: .04em; text-transform: uppercase; }
h2 { font-size: 1.3rem; font-weight: 400; margin: 3rem 0 .8rem; padding-top: .8rem; border-top: 1px solid var(--ink);
     text-transform: uppercase; letter-spacing: .04em; }
h2 .num { color: var(--river-2); margin-right: .5em; font-variant-numeric: tabular-nums; }
h3 { font-size: 1.1rem; font-weight: 600; margin: 2rem 0 .5rem; }
p, li { max-width: 65ch; }
p { margin: 0 0 1rem; }
ul, ol { padding-left: 1.3rem; margin: 0 0 1rem; }
li { margin: .2rem 0; }
li::marker { color: var(--river-2); }
strong { font-weight: 600; }
a { color: var(--ink); text-decoration-color: var(--river-2); text-decoration-thickness: 2px; text-underline-offset: 3px; }
a:hover { background: var(--green-1); }
a:focus-visible { outline: 2px solid var(--river-2); outline-offset: 2px; }
code, pre { font-family: ui-monospace, Menlo, Consolas, monospace; font-size: .86em; }
code { background: var(--sheet); padding: .08em .35em; border: 1px solid var(--rule); }
pre { background: var(--sheet); border: 1px solid var(--ink); padding: 14px 16px; overflow-x: auto; line-height: 1.45; margin: 0 0 1.2rem; }
pre code { background: none; padding: 0; border: 0; font-size: inherit; }
hr { display: none; }
.table { overflow-x: auto; margin: 0 0 1.3rem; }
table { border-collapse: collapse; width: 100%; font-size: .95rem; font-variant-numeric: tabular-nums; }
th, td { text-align: left; vertical-align: top; padding: .45rem .7rem .45rem 0; border-bottom: 1px solid var(--rule); }
th { font-weight: 400; text-transform: uppercase; letter-spacing: .06em; font-size: .8rem; color: var(--muted); border-bottom-color: var(--ink); }
td:first-child { white-space: nowrap; }
.lede { margin-bottom: 1.6rem; }
.status { background: var(--sheet); border: 1px solid var(--ink); padding: 12px 16px; }
.status p { margin: 0; }
.site { display: flex; flex-wrap: wrap; align-items: baseline; gap: .3rem 1.2rem; margin: 0 0 1.6rem; padding-bottom: .5rem; border-bottom: 1px solid var(--ink); }
.site .home { font-size: 1.05rem; font-weight: 400; letter-spacing: .08em; color: var(--ink); text-decoration: none; }
.site .home:hover { color: var(--river-2); background: none; }
.site nav { margin-left: auto; display: flex; flex-wrap: wrap; gap: .2rem 1rem; font-size: .92rem; }
.site nav a { color: var(--ink); text-decoration-color: var(--river-2); text-underline-offset: 3px; }
.site nav a[aria-current] { text-decoration: none; color: var(--river-2); }
a.home { color: inherit; text-decoration: none; }
a.home:hover { background: none; color: var(--river-2); }
main > a.home { display: inline-block; font-size: .9rem; letter-spacing: .08em; text-transform: uppercase; color: var(--muted); margin: 0 0 .4rem; }
.callout { border-left: 4px solid var(--river); background: var(--sheet); padding: 10px 16px; margin: 0 0 1.2rem; max-width: 65ch; }
.callout p { margin: 0; }
details { border: 1px solid var(--ink); background: var(--sheet); padding: 0 16px; margin: 0 0 1.2rem; }
details > summary { cursor: pointer; padding: 10px 0; }
details[open] > summary { margin-bottom: .4rem; }
details > :last-child { margin-bottom: 1rem; }
.swatch { display: inline-block; width: .7em; height: .7em; margin-right: .4em; vertical-align: .02em; box-shadow: 0 0 0 1px var(--ink); }
nav.contents ol { columns: 2 14rem; column-gap: 2rem; padding-left: 1.3rem; }
.figs { display: flex; flex-wrap: wrap; gap: 20px; margin: .4rem 0 1.6rem; }
figure { margin: 0; flex: 0 1 auto; }
figure img { display: block; max-width: 100%; height: auto; }
figure.shot img { width: 300px; image-rendering: pixelated; border: 1px solid var(--ink); }
figure.wide { flex-basis: 100%; }
figure.wide img { width: 100%; }
figcaption { color: var(--muted); font-size: .88rem; margin-top: .4rem; max-width: 300px; }
figure.wide figcaption { max-width: none; }
nav.contents li { break-inside: avoid; }
@media (prefers-reduced-motion: no-preference) { html { scroll-behavior: smooth; } }
"""


def inline(t):
    t = html.escape(t, quote=False)
    t = re.sub(r"`([^`]+)`", r"<code>\1</code>", t)
    t = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", t)
    t = re.sub(r"(?<![\w*])\*([^*\s][^*]*)\*(?![\w*])", r"<em>\1</em>", t)
    t = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", lambda m: f'<a href="{m.group(2)}">{m.group(1)}</a>', t)
    for name, col in PART_COLOURS.items():          # a part named at the start of a table cell gets its swatch
        t = re.sub(rf"^<strong>{name}</strong>", f'<span class="swatch" style="background:{col}"></span><strong>{name}</strong>', t)
    return t


def slug(title):
    m = re.match(r"(\d+)\.", title)
    return f"s{m.group(1)}" if m else re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")


NAME = "AMB-1"
IMG_BASE = SRC / "docs"
EMBED = False                                         # --fragment: images as data URIs


def figure(caption, src):
    path = IMG_BASE / src
    if EMBED:
        mime = "image/svg+xml" if src.endswith(".svg") else "image/png"
        url = f"data:{mime};base64," + base64.b64encode(path.read_bytes()).decode()
    else:
        url = src
    cls = "wide" if src.endswith(".svg") else "shot"
    return (f'<figure class="{cls}"><img src="{url}" alt="{html.escape(caption)}" loading="lazy">'
            f"<figcaption>{inline(caption)}</figcaption></figure>")


def convert(md):
    lines = md.splitlines()
    out, i, contents, sections = [], 0, None, {}
    for ln in lines:                                  # section numbers -> ids, for the contents
        m = re.match(r"## (\d+)\. (.+)", ln)
        if m:
            sections[m.group(2).strip()] = f"s{m.group(1)}"
    while i < len(lines):
        ln = lines[i]
        if ln.startswith("```"):
            j = i + 1
            while j < len(lines) and not lines[j].startswith("```"):
                j += 1
            out.append("<pre><code>" + html.escape("\n".join(lines[i + 1:j]), quote=False) + "</code></pre>")
            i = j + 1
            continue
        if ln.startswith("!["):                      # consecutive image lines: one row of figures
            figs = []
            while i < len(lines) and (lines[i].startswith("![") or (not lines[i].strip() and i + 1 < len(lines)
                                                                    and lines[i + 1].startswith("!["))):
                m = re.match(r"!\[(.*)\]\((.*)\)", lines[i])
                if m:
                    figs.append(figure(m.group(1), m.group(2)))
                i += 1
            out.append('<div class="figs">' + "".join(figs) + "</div>")
            continue
        if ln.strip().startswith("<details>"):          # a toggle: <details><summary>…</summary> … </details>
            m = re.match(r"\s*<details>\s*<summary>(.*)</summary>\s*$", ln)
            out.append(f"<details><summary>{inline(m.group(1)) if m else ''}</summary>")
            i += 1
            continue
        if ln.strip() == "</details>":
            out.append("</details>")
            i += 1
            continue
        if ln.startswith("> "):                           # a callout: the important bit
            para = []
            while i < len(lines) and lines[i].startswith(">"):
                para.append(lines[i][1:].strip())
                i += 1
            out.append(f'<div class="callout"><p>{inline(" ".join(para))}</p></div>')
            continue
        if ln.startswith("# "):                         # the title; AMB-1's own a way home (the site's root)
            t = ln[2:].strip()
            if t == NAME:                               # the site's header (home, the other pages), then the title
                out.append('<header class="site"><a class="home" href="../">AMB-1</a><nav><a href="../emu/">Emulator</a><a href="../install/">Install</a><a href="../manual/" aria-current="page">Manual</a><a href="https://github.com/charlesvestal/fm1-amb">Source</a></nav></header>')
                out.append('<h1>Manual</h1>')
            else:
                out.append(f'<a class="home" href="../">{NAME}</a>\n<h1>{inline(t)}</h1>')
        elif ln.startswith("## "):
            t = ln[3:].strip()
            m = re.match(r"(\d+)\. (.+)", t)
            if t == "Contents":
                contents = True
                out.append('<nav class="contents" aria-label="Contents"><h2 id="contents">Contents</h2>')
            elif m:
                out.append(f'<h2 id="s{m.group(1)}"><span class="num">{m.group(1)}</span>{inline(m.group(2))}</h2>')
            else:
                out.append(f'<h2 id="{slug(t)}">{inline(t)}</h2>')
        elif ln.startswith("### "):
            out.append(f'<h3 id="{slug(ln[4:])}">{inline(ln[4:])}</h3>')
        elif ln.strip() == "---":
            if contents:
                out.append("</nav>")
                contents = None
            out.append("<hr>")
        elif ln.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            head, body = rows[0], [r for r in rows[1:] if not set("".join(r)) <= set("-: ")]
            out.append('<div class="table"><table><thead><tr>' + "".join(f"<th>{inline(c)}</th>" for c in head) +
                       "</tr></thead><tbody>" + "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>"
                                                         for r in body) + "</tbody></table></div>")
            continue
        elif re.match(r"(\d+\.|-) ", ln):
            ordered = ln[0].isdigit()
            items = []
            while i < len(lines) and (re.match(r"(\d+\.|-) ", lines[i]) or (lines[i].startswith("  ") and items)):
                if lines[i].startswith("  "):
                    items[-1] += " " + lines[i].strip()
                else:
                    items.append(re.sub(r"^(\d+\.|-) ", "", lines[i]))
                i += 1
            if contents:
                items = [f'<a href="#{sections.get(t.strip(), slug(t))}">{inline(t)}</a>' for t in items]
            else:
                items = [inline(t) for t in items]
            tag = "ol" if ordered else "ul"
            out.append(f"<{tag}>" + "".join(f"<li>{t}</li>" for t in items) + f"</{tag}>")
            continue
        elif ln.strip():
            para = [ln]
            while i + 1 < len(lines) and lines[i + 1].strip() and not re.match(r"(#|\||```|---|\d+\. |- |!\[|> |\s*</?details)", lines[i + 1]):
                i += 1
                para.append(lines[i])
            text = inline(" ".join(p.strip() for p in para))
            if text.startswith("<strong>Status.</strong>"):
                out.append(f'<div class="status"><p>{text}</p></div>')
            elif out and out[-1].startswith("<h1>"):
                out.append(f'<p class="lede">{text}</p>')
            else:
                out.append(f"<p>{text}</p>")
        i += 1
    return "\n".join(out)


def main():
    args = [a for a in sys.argv[1:] if a != "--fragment"]
    global EMBED
    fragment = "--fragment" in sys.argv
    EMBED = fragment
    src = Path(args[0]) if args else SRC / "docs" / "MANUAL.md"
    dst = Path(args[1]) if len(args) > 1 else SRC / "docs" / "manual.html"
    md = src.read_text()
    h1 = re.search(r"^# (.+)$", md, re.M)
    title, what = (f"{NAME} Manual", "The manual") if not h1 or h1.group(1) == NAME else (f"{NAME}: {h1.group(1)}", h1.group(1))
    body = convert(md)
    page = (f"<title>{html.escape(title)}</title>\n"
            f'<meta name="description" content="{html.escape(what)} for {NAME}, a generative ambient firmware for the M-VAVE FM-1.">\n'
            f"<style>{CSS}</style>\n<main>\n{body}\n</main>\n")
    if not fragment:
        head, _, rest = page.partition("<main>")
        page = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
                '<meta name="viewport" content="width=device-width, initial-scale=1">\n' + head +
                "</head>\n<body>\n<main>" + rest + "</body>\n</html>\n")
    dst.write_text(page)
    print(f"{dst}: {len(page)} bytes")


if __name__ == "__main__":
    main()
