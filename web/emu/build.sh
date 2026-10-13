#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# AMB-1 in the browser: the host simulator compiled to WebAssembly (Emscripten), plus its page.
#   [EN_VERSION=0.1] web/emu/build.sh  ->  build/emu/{index.html, worklet.js, amb.wasm}
# Same sources and flags as host/build_host.sh (-ffp-contract=off, like the device).
set -e
cd "$(dirname "$0")/../.."
mkdir -p build/gen build/emu
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable \
    -DEN_HOST -DEN_WEB "-DEN_VERSION=\"$(printf %s "${EN_VERSION:-DEV}" | tr a-z A-Z)\"" -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/amb.wasm web/emu/amb_web.c firmware/src/dsp/amb.c
cp web/emu/index.html web/emu/worklet.js build/emu/
echo "emu: build/emu ($(wc -c < build/emu/amb.wasm) B wasm)"
