#!/usr/bin/env bash
# pico-c regression suite.
#
#   test_common.sh <platform> [cart ...]
#
# Runs tests/tools/run_tests.py, the one implementation of the suite (it
# needs Python 3 and nothing else, so it also runs on Windows without a
# shell): every cart in tests/carts with a golden transcript runs headless
# and is compared with it, and with no cart given the audio suite in
# tests/audio runs as well.  The golden transcripts and the audio
# expectations were recorded from the real PICO-8, so this checks pico-c
# against PICO-8 on every platform.
#
# Environment:
#   PICO_C_BIN   path of the pico-c binary (default build/<platform>/pico-c)
#
# pico-c 회귀 스위트. 스위트의 유일한 구현인 tests/tools/run_tests.py를 부른다
# (Python 3만 있으면 되므로 셸 없는 Windows에서도 돈다).

set -uo pipefail

PLATFORM="${1:-}"
if [ -z "$PLATFORM" ]; then
    echo "usage: $(basename "$0") <platform> [cart ...]" >&2
    exit 2
fi
shift

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
. "$SCRIPT_DIR/common.sh"

if [ -n "${PICO_C_BIN:-}" ]; then
    BIN="$PICO_C_BIN"
else
    BIN="$PC_ROOT/build/$PLATFORM/pico-c"
    [ "$PLATFORM" = "windows" ] && BIN="$BIN.exe"
fi

if [ ! -x "$BIN" ]; then
    pc_die "pico-c binary not found: $BIN
Build it first:  make PLATFORM=$PLATFORM"
fi

PY="$(pc_python)"
[ -n "$PY" ] || pc_die "python3 is needed to run the suite"

exec "$PY" "$PC_ROOT/tests/tools/run_tests.py" --bin "$BIN" "$@"
