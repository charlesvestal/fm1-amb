# AMB-1 on the M-Vave SMK-37

AMB-1 also builds for the M-Vave SMK-37, a 37-key controller with the same chip as the FM-1. The FM-1 build is
unchanged; everything for the SMK-37 is behind `BOARD_SMK37`. On the SMK-37 it is the same AMB-1: the same
garden, pieces, pages, screen and manual.

Until this project's releases include SMK-37 packages, unofficial builds can be installed with the web installer:
https://genkigenki.github.io/smk37/

## Supported models

- **SMK-37 Pro**: the default build (package identity `SMK-37 Pro_700`).
- **SMK-37 Elite**: `MODEL=elite` (identity `SMK-37 Elite_700`).
- **MKE-P37**: `MODEL=mke` (identity `MKE-P37_700`).
- **Starrykey 37 Play**: uses the default build.

All of them are the same hardware. Each build carries its model's name, so M-Vave's updater still takes that
unit's stock package later: install the package for your model. The installers take a package for any of these
models, and refuse an FM-1 package (and an SMK package on an FM-1).

## Controls

The FM-1's controls keep their AMB-1 meaning, so the manual applies; the screen keeps the FM-1's names (HOME,
SEL, ENV, ...). The SMK-37 adds the rest.

| SMK-37 | AMB-1 |
| --- | --- |
| All 37 keys | the scale, as the FM-1's keys: each adds its note or takes it out (the outer keys C3..E3 and G#5..C6 too) |
| 16 pads | the bells: sixteen steps of the scale up from the chord's root, low steps on the bottom row (left to right), the higher ones on the top row. A pad rings a bell there, with velocity. The chord's notes glow green (its root brightest), frost blue while frozen; a bell from a loop lights its step in that loop's shade |
| K1 | MASTER (volume) |
| K2 / K3 / K4 | SELECT / ALGORITHM / PRESETS |
| K5..K8 | KNOB 1..4 |
| PATCH | HOME (the Garden page) |
| CHORD / ARP / NOTE REPEAT / SEQ | ENV / ARP / LFO / SEQ: the Pad, Bells, Bass and Pulse pages |
| FX / GLOBE | FX / GLO: the Space and Setup pages |
| PARA | EDIT: the next chord |
| SEQ PLAY / SEQ REC | PLAY / REC: grow or rest / freeze |
| BT | SAVE |
| OCTAVE- / OCTAVE+ | OCT- / OCT+: the bells' octave |
| SCALE | SEL (no job, as on the FM-1) |
| < / > | the previous / next page, as SELECT |
| KNOB BANK | the picker: the pads and the screen show the eight pieces (top row) and the eight modes (bottom row); a pad picks. Lit while it is open; press again to close it. It also shows for a moment after a PRESETS or ALGORITHM turn |
| FADER BANK | what the faders set: A the levels (Pad, Bells, Bass, Pulse), B the space (Echo, Echo time, Reverb, Size), C the garden (Evolve, Motion, Drift, Density). Lit off bank A. The fader card shows the bank and its values |
| Faders 1..4 | the current bank's four values; a fader takes over only once it moves (also after a bank change) |
| Pitch wheel | up: more echo (a throw); down: more reverb (a wash). Back at rest, the sends are as set |
| Mod wheel | Bright, added to the pad's |
| OCTAVE- + OCTAVE+ held 5 s | UPDATE MODE |
| OCTAVE- + OCTAVE+ held at power-on | HARDWARE CALIBRATION: teach the buttons and knobs again (it asks by the FM-1's names) |

Only one card is up at a time: KNOB BANK closes the fader card, FADER BANK closes the picker. The wheels never
change what is saved. Lights Off (Setup page) darkens the pads; the SMK-37 has no key lights and no dim button
glow, so On and Keys look the same there.

The SMK-37's TRS jack is a MIDI output and this build does not use it; there is no TRS MIDI IN, so MIDI and its
clock come in over USB only.

## Build

See [BUILDING.md](BUILDING.md) for the toolchain.

```sh
BOARD=smk37 ./build.sh                 # build/amb-smk37.fwsc (SMK-37 Pro, Starrykey 37 Play)
BOARD=smk37 MODEL=elite ./build.sh     # build/amb-smk37-elite.fwsc
BOARD=smk37 MODEL=mke ./build.sh       # build/amb-smk37-mke.fwsc
./build.sh                             # the FM-1 build again before tests/run_tests.sh (the tests use its files)
```

## Install

Connect the keyboard by USB and run, with the package for your model:

```sh
python3 tools/fm1_install.py build/amb-smk37.fwsc
```

`fm1_install.py` needs `mido` and `python-rtmidi` (`pip3 install mido python-rtmidi`). It checks that the package
is for the connected keyboard.

## Going back to the stock firmware

Install M-Vave's firmware package for your model with M-Vave's updater, M-UPGRADE. It works directly, because
the installed package keeps the keyboard's model name.

## Safety

- Back up first: have M-Vave's stock package for your model and M-UPGRADE ready before you install.
- Keep the USB cable connected until the install has finished and the keyboard has restarted.
- The first start clears the stock firmware's settings, and any other firmware's data, in the shared data area.
- The firmware never writes the keyboard's calibration, Bluetooth pairing or key areas; the flash map refuses
  them at compile time and again in the flash driver.
- Installing other firmware is at your own risk.
