# Building AMB-1

AMB-1 is an app on [fm1-base](https://github.com/charlesvestal/fm1-base) (`base/`, a git submodule, with
felucca-bsp inside it). Clone with `--recursive`, or after cloning:

```
git submodule update --init --recursive
```

`./build.sh` runs fm1-base's build (`base/tools/build.py`) on `app/`, whose settings are in `app/fm1.toml`. It
makes, in `app/build/`:

| File | What |
| --- | --- |
| `amb.bin` | the firmware app |
| `loader/ota.bin` | the update loader (felucca-bsp's) |
| `amb.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow` (Homebrew's Python, with libraqm, for the fonts).
- Docker Desktop, or podman with a `docker` script that runs `exec podman "$@"` first on your PATH. The JieLi
  toolchain is Linux x86-64 only; the build runs it in a `linux/amd64` `debian:bookworm-slim` container
  (Rosetta on Apple silicon). Keep the source tree in a folder the container can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server): see fm1-base's README.
- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files (`cpu/wl82/tools/uboot.boot`,
  `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); none of it is part of this tree or of git.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Emscripten (optional, for the browser builds) and Node.js (optional, for their tests).

On Linux x86-64 the toolchain runs natively and no container is needed.

## Build

```
./build.sh                    # app/build/amb.fwsc
./build.sh --release X.Y      # app/build/amb-X.Y.fwsc, identity FM-1_7XXYYZZ
./build.sh --host             # the simulator: app/build/host/amb_host
./build.sh --web              # the browser emulator: app/build/emu/
web/app/build.sh              # the web app (the engine alone, for phone and desktop): app/build/app/
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations (`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`),
`PYTHON` the Python that runs the build. The build also checks the image, RAM and pool sizes, that register access
stays in felucca-bsp, and that no double-precision code is linked.

## Tests

```
sh tests/run_tests.sh             # AMB-1's
sh base/tests/run_tests.sh        # fm1-base's own
```

AMB-1's: the instrument (`tests/host/amb_test.c`); the update path against `app/build/amb.fwsc`; the browser
builds in Node; and the whole app in the simulator (`tests/scenarios/*.amb`: the pieces, the scale, Motion, the
help cards, the screen reader, an AMB-1 0.9 project carried over), with audio and screenshots in
`app/build/scenarios/`.

`app/build/host/amb_host SCRIPT OUTDIR` runs one script. The commands are listed at the top of
`base/host/plat_host.c`, and AMB-1's own checks and commands at the top of `app/host.c`.

## Install

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 base/tools/fm1_install.py app/build/amb.fwsc
```

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for the update mode. If the FM-1 no
longer starts but reaches the chip's update mode (4C4A:8057 on USB), `base/bsp/tools/fm1_rescue.sh` puts stock
firmware back from a Mac.
