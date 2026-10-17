#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""The manual's map of the controls, drawn (no photographs: M-VAVE's and Felucca's are not ours to use):

  docs/img/fm1-panel.svg   the FM-1 front panel, AMB-1's real screen in it, each control labelled with
                           what it does in AMB-1, the keys with their notes

  tools/gen_manual_figures.py [SCREEN.png]     (default docs/img/screen-garden.png)

From X0X's: the panel layout follows the FM-1: two knob pairs and the octave buttons on the left, the
screen in the middle, four knobs and two rows of six buttons on the right, and the keys below: 11
black keys in groups of 3, 2, 3, 2, 1 over 16 white keys (F to G). The words under the controls are in
the rivers' blue of the Music for Airports cover."""
import base64
import io
import sys
from pathlib import Path

from PIL import Image

SRC = Path(__file__).resolve().parents[1]
OUT = SRC / "docs" / "img"
FONT = "'Helvetica Neue', Helvetica, Arial, sans-serif"

BODY, TRAY, KEY_W, KEY_B, KNOB, BTN, LABEL = "#3a3d42", "#26282c", "#d9dadc", "#c9cacd", "#1f2124", "#2e3135", "#e8e8ea"
WHAT = "#9fd3cf"
WHITE_N = 16
BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]          # black key k sits after white key BLACK_AFTER[k]
WHITE_NOTES = "F G A B C D E F G A B C D E F G".split()
BLACK_NOTES = "F# G# A# C# D# F# G# A# C# D# F#".split()


def png_data_uri(path, scale=1):
    im = Image.open(path).convert("RGB")
    if scale != 1:
        im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    b = io.BytesIO()
    im.save(b, "PNG", optimize=True)
    return "data:image/png;base64," + base64.b64encode(b.getvalue()).decode()


def what(x, y, t):
    """what a control does, under it (one or two lines, split at |)"""
    a, _, b = t.partition("|")
    return (f'<text x="{x}" y="{y}" text-anchor="middle" class="what">{a}</text>' +
            (f'<text x="{x}" y="{y + 15}" text-anchor="middle" class="what">{b}</text>' if b else ""))


def knob(x, y, r, label, does=None):
    """a knob: its name above, what it does in AMB-1 below"""
    return (f'<g><circle cx="{x}" cy="{y}" r="{r + 5}" fill="{TRAY}"/>'
            f'<circle cx="{x}" cy="{y}" r="{r}" fill="{KNOB}" stroke="#44474c" stroke-width="2"/>'
            f'<line x1="{x}" y1="{y - r + 5}" x2="{x}" y2="{y - r + 15}" stroke="{LABEL}" stroke-width="3" stroke-linecap="round"/>'
            f'<text x="{x}" y="{y - r - 12}" text-anchor="middle" class="lbl">{label}</text>'
            + (what(x, y + r + 22, does) if does else "") + '</g>')


def button(x, y, w, h, label, two=None):
    t = (f'<text x="{x + w / 2}" y="{y + h / 2 + 5}" text-anchor="middle" class="btn">{label}</text>' if not two else
         f'<text x="{x + w / 2}" y="{y + h / 2 - 1}" text-anchor="middle" class="btn small">{label}</text>'
         f'<text x="{x + w / 2}" y="{y + h / 2 + 12}" text-anchor="middle" class="btn small">{two}</text>')
    return f'<g><rect x="{x}" y="{y}" width="{w}" height="{h}" rx="9" fill="{BTN}" stroke="#4a4d52" stroke-width="1.5"/>{t}</g>'


def keybed(x0, y0, w, h):
    """the 16 white keys and 11 black keys as pills, each with its note"""
    s = [f'<rect x="{x0}" y="{y0}" width="{w}" height="{h}" rx="18" fill="{TRAY}"/>']
    pitch = (w - 24) / WHITE_N
    kw = pitch * 0.78
    for i in range(WHITE_N):
        x = x0 + 12 + i * pitch + (pitch - kw) / 2
        s.append(f'<rect x="{x:.1f}" y="{y0 + h * 0.43:.1f}" width="{kw:.1f}" height="{h * 0.52:.1f}" rx="{kw / 2:.1f}" fill="{KEY_W}"/>')
        s.append(f'<text x="{x + kw / 2:.1f}" y="{y0 + h * 0.43 + h * 0.52 * 0.72:.1f}" text-anchor="middle" class="key">{WHITE_NOTES[i]}</text>')
    for k, a in enumerate(BLACK_AFTER):
        x = x0 + 12 + (a + 1) * pitch - kw / 2
        s.append(f'<rect x="{x:.1f}" y="{y0 + h * 0.06:.1f}" width="{kw:.1f}" height="{h * 0.33:.1f}" rx="{kw / 2:.1f}" fill="{KEY_B}"/>')
        s.append(f'<text x="{x + kw / 2:.1f}" y="{y0 + h * 0.06 + h * 0.33 * 0.62:.1f}" text-anchor="middle" class="key small">{BLACK_NOTES[k]}</text>')
    return "".join(s)


STYLE = (f"<style>.lbl{{font:600 16px {FONT};fill:{LABEL};letter-spacing:.06em}}"
         f".btn{{font:600 14px {FONT};fill:{LABEL};letter-spacing:.05em}}.btn.small{{font-size:11px}}"
         f".key{{font:600 16px {FONT};fill:#2a2c30}}.key.small{{font-size:13px}}"
         f".what{{font:400 13px {FONT};fill:{WHAT}}}</style>")


def panel(screen_png):
    W, H = 1000, 720
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" role="img" '
         f'aria-label="The FM-1 front panel running AMB-1, each control labelled with what it does">', STYLE,
         f'<rect x="4" y="4" width="{W - 8}" height="{H - 8}" rx="44" fill="{BODY}"/>',
         knob(105, 92, 24, "MASTER", "Volume"), knob(215, 92, 24, "SELECT", "Page|HOME + it: tempo"),
         knob(105, 210, 24, "PRESETS", "Piece"), knob(215, 210, 24, "ALGORITHM", "Mode"),
         f'<rect x="58" y="284" width="210" height="58" rx="14" fill="{TRAY}"/>',
         button(72, 294, 82, 38, "OCT-"), button(170, 294, 82, 38, "OCT+"),
         what(113, 362, "Bells down"), what(211, 362, "Bells up"),
         f'<rect x="300" y="44" width="236" height="236" rx="26" fill="#111214"/>',
         f'<image x="318" y="62" width="200" height="200" href="{png_data_uri(screen_png)}" style="image-rendering:pixelated"/>']
    for i in range(4):
        s.append(knob(610 + i * 104, 92, 22, f"KNOB{i + 1}"))
    s.append(what(766, 136, "The page's four values (KNOB1: a layer's level)"))
    s.append(f'<rect x="568" y="146" width="400" height="236" rx="18" fill="{TRAY}"/>')
    top = [("FX", "Space"), ("SEL", ""), ("ENV", "Pad"), ("LFO", "Bass"), ("EDIT", "Next|chord"), ("GLO", "Setup")]
    bot = [("HOME", "Garden"), ("SAVE", "Save, hold:|recipe"), ("ARP", "Bells"), ("SEQ", "Pulse"),
           (("PLAY", "STOP"), "Grow /|rest"), ("REC", "Freeze")]
    for i in range(6):
        x = 582 + i * 63
        s.append(button(x, 158, 54, 48, top[i][0]))
        if top[i][1]:
            s.append(what(x + 27, 224, top[i][1]))
        b = bot[i][0]
        s.append(button(x, 264, 54, 48, b[0], b[1]) if isinstance(b, tuple) else button(x, 264, 54, 48, b))
        if bot[i][1]:
            s.append(what(x + 27, 330, bot[i][1]))
    s.append(keybed(36, 400, W - 72, 270))
    s.append(what(W / 2, 698, "Every key: its note into the scale, or out of it (lit: in the scale)   ·   "
                              "Hold any button a second: what it does"))
    s.append("</svg>")
    return "".join(s)


def main():
    screen = Path(sys.argv[1]) if len(sys.argv) > 1 else OUT / "screen-garden.png"
    (OUT / "fm1-panel.svg").write_text(panel(screen))
    print(f"figure: {OUT / 'fm1-panel.svg'}")


if __name__ == "__main__":
    main()
