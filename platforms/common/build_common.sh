#!/usr/bin/env bash
# Shared build driver used by platforms/<platform>/build.sh.
#
#   build_common.sh <platform> [make args...]
#
# Checks the toolchain, then hands over to the top level Makefile.  All the
# artifacts end up in build/<platform>/.

set -euo pipefail

PLATFORM="${1:-}"
if [ -z "$PLATFORM" ]; then
    echo "usage: $(basename "$0") <platform> [make args...]" >&2
    exit 2
fi
shift

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
. "$SCRIPT_DIR/common.sh"

cd "$PC_ROOT"

pc_info "$PLATFORM"

if ! command -v make >/dev/null 2>&1; then
    pc_die "GNU make not found in PATH"
fi

case "$PLATFORM" in
    macos)
        if [ ! -d /opt/local/include/SDL2 ]; then
            pc_die "SDL2 not found in /opt/local
Install it with:  sudo port install sdl2"
        fi
        export CXX="${CXX:-clang++}"
        export AR="${AR:-ar}"
        export SDL2_DIR="${SDL2_DIR:-/opt/local}"
        ;;
    linux)
        if [ ! -d /usr/include/SDL2 ]; then
            pc_die "SDL2 headers not found in /usr/include/SDL2
Install them with:  sudo apt install libsdl2-dev"
        fi
        export CXX="${CXX:-g++}"
        export AR="${AR:-ar}"
        export SDL2_DIR="${SDL2_DIR:-/usr}"
        ;;
    windows)
        export CXX="${CXX:-x86_64-w64-mingw32-g++}"
        export AR="${AR:-x86_64-w64-mingw32-ar}"
        export SDL2_DIR="${SDL2_DIR:-$PC_ROOT/platforms/windows/deps/SDL2-2.30.11/x86_64-w64-mingw32}"
        if [ ! -d "$SDL2_DIR" ]; then
            pc_die "vendored MinGW SDL2 not found: $SDL2_DIR
See platforms/windows/README.md for how to obtain it."
        fi
        ;;
    *)
        pc_die "unknown platform '$PLATFORM'"
        ;;
esac

printf 'building for %s ...\n' "$PLATFORM"
make PLATFORM="$PLATFORM" "$@"
printf 'built %s\n' "$PC_ROOT/build/$PLATFORM/pico-c$([ "$PLATFORM" = windows ] && echo .exe)"
