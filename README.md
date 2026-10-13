# AMB-1 for the M-VAVE FM-1

AMB-1 turns the M-VAVE FM-1 into a generative ambient machine, inspired by Brian Eno's
*Music for Airports*. It plays itself from the moment it's switched on: breathing voices, soft
bells on tape loops of unequal length, a gliding bass and quiet Euclidean percussion, all through
a tape echo, a long tape loop and a plate reverb. You don't perform it, you tend it: pick a piece
and a scale, take notes out, put them back, and let it grow.

![AMB-1's screen: the garden](docs/img/screen-garden.png) ![Voices](docs/img/screen-voices.png) ![Frozen](docs/img/screen-frozen.png)

- **Try it in your browser** (no FM-1 needed): <https://charlesvestal.github.io/fm1-amb/emu/>
- **Install it** (Chrome or Edge, with the FM-1 plugged in): <https://charlesvestal.github.io/fm1-amb/install/>
- **Read the manual**: <https://charlesvestal.github.io/fm1-amb/manual/> (source: [docs/MANUAL.md](docs/MANUAL.md))
- **Download the firmware file**: [releases](https://github.com/charlesvestal/fm1-amb/releases)
- **M-Vave SMK-37:** see [SMK-37 instructions](SMK37.md). Until this project's releases include SMK-37 packages, unofficial builds can be installed with the web installer: https://genkigenki.github.io/smk37/

Installing it is at your own risk, though it's hard to get stuck: the web installer can put
M-VAVE's own firmware back, and an FM-1 that fails to start twice comes up in a safe mode the
installer can reach.

## In short

| | |
|---|---|
| Keys | The scale: each key adds its note or takes it out |
| PLAY · REC · EDIT | Grow / rest · freeze · the next chord |
| HOME · ENV · ARP · LFO · SEQ · FX · GLO | The pages: Garden, Pad, Bells, Bass, Pulse, Space, Setup; KNOB 1–4 set their values, a layer's level on KNOB 1 |
| SELECT | The previous or next page; with HOME held, the tempo |
| ALGORITHM · PRESETS | The mode · the piece (eight of them) |
| OCT− / OCT+ | The bells' octave |
| SAVE | Saves; held, puts the piece back to its recipe |

Hold any button for a second and a card says what it does.

The [manual](https://charlesvestal.github.io/fm1-amb/manual/) has the rest.

## Building and testing

The toolchain is the same as X0X's (see [BUILDING.md](BUILDING.md)):

```
tests/run_tests.sh      # the instrument, the platform pieces, the app in a simulator, the web builds
./build.sh              # build/amb.fwsc
python3 tools/fm1_install.py build/amb.fwsc     # install it over USB
```

`build/host/amb_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.amb`), with audio and screenshots coming out. `web/emu/build.sh` builds the
same code for the browser (the emulator); `web/app/build.sh` builds the engine alone into a web
app for phone and desktop.

## Credits

The idea comes from Brian Eno's generative music: *Music for Airports*' tape loops of unequal
length, and the echo chain of *Discreet Music*. The reverb is Jon Dattorro's plate ("Effect Design,
Part 1", JAES 1997), as in [FoMni-1](https://github.com/charlesvestal/fm1-fomni); the Euclidean
rhythms are Godfried Toussaint's reading of Bjorklund's algorithm. The platform (hardware layer,
USB, update loader, storage, installer) is [Felucca](https://github.com/hugelton/Felucca)'s by Leo
Kuroshita (Hügelton Instruments), by way of [X0X](https://github.com/charlesvestal/fm1-x0x) and
FoMni-1. The idea of running FM-1 firmware in the browser comes from
[Groove OS](https://www.groove-os.com/). GPL-3.0-only; see [LICENSING.md](LICENSING.md).

M-VAVE and FM-1 are trademarks of their owners. AMB-1 is not affiliated with M-VAVE, Brian Eno or
Hügelton Instruments.
