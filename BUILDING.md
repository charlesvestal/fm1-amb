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

## Publishing a release

A release is built on GitHub from its tag, so anyone can check that the package came from this source:
`.github/workflows/release.yml` calls the shared FM-1 builder,
[`zvenson/fm1-build` at `v1`](https://github.com/zvenson/fm1-build/tree/v1) (as LOOM-8, MONK-1, X0X and Melodee do).
The builder checks out the tagged commit, installs JieLi's Linux toolchain and the AC79 SDK at its fixed tag, runs
`./build.sh --release X.Y` (with fm1-base and felucca-bsp at the commits this one links) and `tests/run_tests.sh` (a
failing test stops the release; the browser builds' tests need Emscripten, which the builder does not have, and are
skipped there), then signs build provenance for `amb-X.Y.fwsc` and `amb-X.Y-app.bin`. The Firmware Hub shows such
packages as built from source.

1. Write the notes first, as a draft: `gh release create vX.Y --draft --title "AMB-1 X.Y" --notes-file NOTES.md`
   (the workflow keeps a draft's notes; without one it makes a draft from the commits).
2. Push the tag: `git tag vX.Y && git push origin vX.Y`. The workflow builds, tests, attests, checks the SHA-256 sums
   and the signatures against the builder, the tag and the commit, then publishes the draft. A tag with a suffix
   (`v1.4-beta`) is a prerelease. A published release is never rebuilt: a new build needs a new tag. A manual run
   on a branch (Actions, Run workflow) builds and attests a development package, in the run's `packages` artifact.
3. The site from that package: `gh release download vX.Y --pattern '*.fwsc' --dir app/build --clobber`, then
   `tools/publish_pages.sh X.Y`: it builds the browser emulator and the site (landing page, emulator, fm1-base's web
   installer, manual, the words files, the firmware) with `tools/make_pages.py` from `app/build/amb-X.Y.fwsc`, and
   pushes it to the `gh-pages` branch, which GitHub Pages serves at <https://charlesvestal.github.io/fm1-amb/>.

To check a downloaded package yourself (GitHub CLI, signed in):

```sh
tag=vX.Y repo=charlesvestal/fm1-amb
gh release download "$tag" --repo "$repo" --dir build/verify --pattern '*.fwsc'
git fetch origin "refs/tags/$tag:refs/tags/$tag"
gh attestation verify "build/verify/amb-${tag#v}.fwsc" --repo "$repo" \
  --signer-workflow zvenson/fm1-build/.github/workflows/build.yml \
  --source-ref "refs/tags/$tag" --source-digest "$(git rev-parse "$tag^{commit}")"
```

The same checks the `-app.bin`. `SHA256SUMS` comes with the downloads; a checksum alone proves nothing about where a
package was built.

A local release build (`./build.sh --release X.Y`, package `app/build/amb-X.Y.fwsc`; its identity FM-1_7XXYYZZ is
what the installer checks) is still how to try one on the FM-1 before tagging.
