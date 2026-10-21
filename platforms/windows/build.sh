#!/usr/bin/env bash
# Build pico-c on Windows (MSYS2 / MinGW-w64).
#
# The MinGW SDL2 lives in platforms/windows/deps/SDL2-2.30.11.  The binary
# lands in build/windows/pico-c.exe.
#
#   platforms/windows/build.sh            # build
#   platforms/windows/build.sh -j8        # build in parallel
#   platforms/windows/build.sh clean      # remove build/windows
#
# The same build is available as a Windows batch file: build.bat

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec bash "$SCRIPT_DIR/../common/build_common.sh" windows "$@"
