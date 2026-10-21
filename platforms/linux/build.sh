#!/usr/bin/env bash
# Build pico-c on Linux.
#
# SDL2 comes from the system (apt install libsdl2-dev).  The binary lands in
# build/linux/pico-c.
#
#   platforms/linux/build.sh            # build
#   platforms/linux/build.sh -j8        # build in parallel
#   platforms/linux/build.sh clean      # remove build/linux

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec bash "$SCRIPT_DIR/../common/build_common.sh" linux "$@"
