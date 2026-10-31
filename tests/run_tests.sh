#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
# AMB-1's tests: the instrument (tests/host/amb_test.c), the whole app in fm1-base's simulator (tests/scenarios/*.amb:
# UI, chords, rhythm, persistence, the screen reader), the browser builds, and the update path against AMB-1's package
# (./build.sh first). Screenshots and audio in app/build/scenarios/. fm1-base's own tests: base/tests/run_tests.sh.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"   # the package's SDK files, as ./build.sh
B=app/build
OUT=$B/tests
FAIL=0
mkdir -p "$OUT" $B/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run amb sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Iapp/dsp -Ibase/include \
    -o $OUT/amb_test tests/host/amb_test.c app/dsp/amb.c -lm && $OUT/amb_test"
if [ -f $B/amb.fwsc ]; then
    run ota-entry sh -c "$CC -w -o $OUT/ota_test base/tests/ota_test.c && $OUT/ota_test $B/amb.fwsc"
    run update-loader sh -c "head -c 100000 $B/amb.bin > $OUT/old_app.bin && \
        $PY base/tools/fm1pkg_make.py $OUT/old_app.bin $B/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -w -o $OUT/ldr_test base/tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc $B/amb.fwsc"
else
    echo "  skip update path (no $B/amb.fwsc: ./build.sh)"
fi
run host-build sh build.sh --host
if command -v emcc >/dev/null 2>&1; then
    run emu sh -c "sh build.sh --web && node tests/host/emu_test.mjs $B/emu/app.wasm"
    run app sh -c "sh web/app/build.sh >/dev/null && node tests/host/app_test.mjs $B/app/amb.wasm"
else
    echo "  skip emu, app (no emcc)"
fi
for s in tests/scenarios/*.amb; do
    n=$(basename "$s" .amb)
    mkdir -p "$B/scenarios/$n"
    run "scenario-$n" $B/host/amb_host "$s" "$B/scenarios/$n"
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
