#!/usr/bin/env bash
# Build pico-c on macOS.
#
# SDL2 comes from MacPorts (/opt/local).  The binary lands in
# build/macos/pico-c.
#
#   platforms/macos/build.sh            # build
#   platforms/macos/build.sh -j8        # build in parallel
#   platforms/macos/build.sh clean      # remove build/macos

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec bash "$SCRIPT_DIR/../common/build_common.sh" macos "$@"
