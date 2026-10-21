#!/usr/bin/env bash
# Run the pico-c regression suite on macOS.
#
# Every cart of tests/carts that has a golden transcript in tests/expected is
# run headless and compared with that transcript.  The transcripts were
# produced by the real PICO-8 (see platforms/macos/oracle.sh).
#
#   platforms/macos/test.sh                 # the whole suite
#   platforms/macos/test.sh cmp_all.p8      # a single cart
#   platforms/macos/test.sh --rebuild       # rebuild before running

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../common/common.sh
. "$SCRIPT_DIR/../common/common.sh"

args=()
rebuild=0
for arg in "$@"; do
    case "$arg" in
        --rebuild) rebuild=1 ;;
        *) args+=("$arg") ;;
    esac
done

if [ $rebuild -eq 1 ]; then
    bash "$SCRIPT_DIR/build.sh"
else
    pc_ensure_binary macos >/dev/null
fi

if [ ${#args[@]} -gt 0 ]; then
    exec bash "$SCRIPT_DIR/../common/test_common.sh" macos "${args[@]}"
fi
exec bash "$SCRIPT_DIR/../common/test_common.sh" macos
