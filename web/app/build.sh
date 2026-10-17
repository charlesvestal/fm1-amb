#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
# The AMB-1 web app: the engine (app/dsp/amb.c, as on the FM-1) to WebAssembly, and the page.
#   web/app/build.sh  ->  app/build/app/{index.html, worklet.js, amb.wasm}: a static folder, servable as is
set -e
cd "$(dirname "$0")/../.."
mkdir -p app/build/app
emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wextra -Wno-unused-function \
    -DEN_HOST -Iapp/dsp -Ibase/include \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=262144 -sINITIAL_MEMORY=8388608 -sFILESYSTEM=0 \
    -o app/build/app/amb.wasm web/app/amb_api.c app/dsp/amb.c
cp web/app/index.html web/app/worklet.js app/build/app/
echo "app: app/build/app ($(wc -c < app/build/app/amb.wasm) B wasm)"
