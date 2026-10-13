# Building AMB-1

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `amb.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `amb.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow`
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 0.9` makes a release build; the package is `build/amb-0.9.fwsc`.

On macOS with podman instead of Docker, put a `docker` script that runs `exec podman "$@"` first
on your PATH. `EN_JOBS` (default 4) limits parallel compiles: a podman machine drops
connections when many containers start at once.

Build option: `EN_CDC=1` adds Felucca's USB serial function (off by default: one plain
MIDI interface).

### The M-Vave SMK-37

```
BOARD=smk37 ./build.sh            # or: ./build.sh --board smk37
```

builds `build/amb-smk37.fwsc` (identity `SMK-37 Pro_700`; `--release X.Y` gives `amb-smk37-X.Y.fwsc`,
`SMK-37 Pro_7XXYYZZ`). `MODEL=elite` or `MODEL=mke` builds the same firmware under the SMK-37 Elite's or the
MKE-P37's model name (`build/amb-smk37-elite.fwsc`, identity `SMK-37 Elite_700`; `build/amb-smk37-mke.fwsc`,
`MKE-P37_700`), so M-Vave's updater still takes that unit's factory package; the Starrykey 37 Play uses the
default build. Everything for the SMK-37 sits behind `BOARD_SMK37` (`-DBOARD_SMK37=1` for the app and the
update loader); without `BOARD` the FM-1 build is unchanged. Options for the SMK build: `EN_LCD_BAUD=0..15` (the
LCD SPI divider; default 4 = 12 MHz, the stock firmware's), `FELUCCA_DATA_FAMILY` / `FELUCCA_DATA_UNTAGGED_OK`
(the store owner tag; keep the defaults `AMB1` and `0`). Both boards write the same files in `build/`: build the
FM-1 last before `tests/run_tests.sh`. See [SMK37.md](SMK37.md) for the controls and the install.

The build also generates `build/gen/` (the font). It fails if a soft-double routine is linked
(a `double` crept in) and checks the image, RAM and pool sizes.

## Tests

```
tests/run_tests.sh
```

Runs, on the build machine (after an FM-1 `./build.sh`, with `AC79_SDK` set for the update-path and
SMK-37 package tests): the maths library against libm; the instrument (`tests/host/amb_test.c`:
Euclidean patterns, scales and chords, voice leading, the render's levels, drift, evolving loops, freeze); flash
storage; Felucca's update-path tests against `build/amb.fwsc`; and the whole app in the simulator
(`tests/scenarios/*.amb`), with its screenshots and audio in `build/scenarios/`; the SMK-37 build's
flash map, packages and update path, and its controls in the simulator (`tests/scenarios/smk37/*.amb`).

`host/build_host.sh` builds the simulator alone; `build/host/amb_host SCRIPT OUTDIR` runs one
script (the command list is at the top of `host/amb_host.c`). `host/build_host.sh smk37` builds the SMK-37
build's (`build/host/amb_host_smk37`).

## Install

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/amb.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for Felucca's update
mode. If the FM-1 no longer starts but reaches the chip's update mode (4C4A:8057 on USB),
`tools/fm1_rescue.sh` puts stock firmware back from a Mac; otherwise recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Publishing a release

1. `./build.sh --release X.Y` (the identity, FM-1_7XXYYZZ, is what the installer checks).
2. `tools/publish_pages.sh X.Y`: builds the site (landing page, manual, browser emulator, web
   installer, firmware) with `tools/make_pages.py` and pushes it to the `gh-pages` branch, which
   GitHub Pages serves at <https://charlesvestal.github.io/fm1-amb/>. The manual's source is
   `docs/MANUAL.md` (`tools/manual_html.py` turns it into the page).
3. `gh release create vX.Y build/amb-X.Y.fwsc` for the command-line download.
