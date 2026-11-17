#!/usr/bin/env bash
# Shared helpers for the pico-c platform scripts.
#
# Sourced by platforms/<platform>/build.sh, test.sh and oracle.sh so that the
# layout of the repository only has to be described in one place.

# Absolute path of the repository root.
PC_ROOT="${PC_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"

export PC_ROOT

# Byte wise comparison: the transcripts contain arbitrary bytes (the glyph
# tests print raw UTF-8), so the locale must not try to interpret them.
export LC_ALL=C

pc_root()         { printf '%s\n' "$PC_ROOT"; }
pc_tests_dir()    { printf '%s\n' "$PC_ROOT/tests"; }
pc_carts_dir()    { printf '%s\n' "$PC_ROOT/tests/carts"; }
pc_expected_dir() { printf '%s\n' "$PC_ROOT/tests/expected"; }
pc_deps_dir()     { printf '%s\n' "$PC_ROOT/platforms/$1/deps"; }

# Print a Python 3 interpreter that really runs, or nothing.  On Windows the
# "python3" on the PATH is often the Microsoft Store / install manager alias,
# which may have no Python behind it or start installing one into the current
# folder, so "python" is tried first and every candidate is run, not just
# found (a Python 2 "python" fails the version check and is skipped).
# 실제로 실행되는 Python 3 경로를 출력한다(없으면 빈 출력). Windows에서 PATH의
# python3는 Microsoft Store/설치 관리자 별칭인 경우가 많아 실체가 없거나 현재 폴더에
# 파이썬을 설치하기 시작하므로, python을 먼저 시도하고 후보를 실제로 실행해 본다
# (Python 2인 python은 버전 검사에서 걸러진다).
#
# The path printed is the interpreter itself (sys.executable), not the alias:
# the install manager alias reads the "#!/usr/bin/env python3" line of the
# script it runs and installs a runtime into the current folder for it.
# 출력하는 경로는 별칭이 아니라 인터프리터 자체(sys.executable)다. 설치 관리자
# 별칭은 실행하는 스크립트의 "#!/usr/bin/env python3" 줄을 읽고 현재 폴더에
# 런타임을 설치해 버리기 때문이다.
pc_python() {
    local c path exe
    for c in python python3; do
        path="$(command -v "$c" 2>/dev/null)" || continue
        exe="$("$path" -c 'import sys; assert sys.version_info[0] == 3; print(sys.executable)' 2>/dev/null)" || continue
        exe="${exe%$'\r'}"
        [ -n "$exe" ] || continue
        printf '%s\n' "$exe"
        return 0
    done
    return 0
}

pc_die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

# Files a cart creates while it runs.  They are removed before every cart so
# that the oracle and pico-c always start from the same state.
PC_STATE_FILES=(guard.p8 ptest.txt runtest2.txt)

# Files that one cart writes and another cart reads back.  They are only
# removed at the beginning of a suite run, so that the pair keeps working.
PC_TRANSFER_FILES=(p8save_out.p8 p8png_out.p8.png)

# Remove the given files from a directory.
pc_clean_files() {
    local dir="$1" f
    shift
    for f in "$@"; do
        rm -f "$dir/$f"
    done
}

# Print the path of the pico-c binary of a platform, building it first when it
# does not exist yet.
pc_ensure_binary() {
    local platform="$1"
    local bin="$PC_ROOT/build/$platform/pico-c"
    [ "$platform" = "windows" ] && bin="$bin.exe"
    if [ ! -x "$bin" ]; then
        printf 'pico-c is not built yet, building it ...\n' >&2
        bash "$PC_ROOT/platforms/common/build_common.sh" "$platform" >&2
    fi
    printf '%s\n' "$bin"
}

pc_info() {
    printf 'pico-c\n'
    printf '  root     : %s\n' "$PC_ROOT"
    printf '  platform : %s\n' "${1:-unknown}"
    printf '  compiler : %s\n' "${CXX:-default}"
    printf '  SDL2     : %s\n' "${SDL2_DIR:-default}"
}

# Normalise an emulator transcript so that the oracle (the real PICO-8) and
# pico-c can be compared byte for byte:
#
#   * drop the oracle's "RUNNING:" banner and pico-c's "DEBUG:" traces
#   * drop empty lines
#   * rewrite absolute paths into stable placeholders
#
# Both the golden files and the live run go through this function, so the two
# sides always agree on the spelling of a path.
pc_normalize() {
    sed -e '/^RUNNING:/d' \
        -e '/^DEBUG/d' \
        -e '/^[[:space:]]*$/d' \
        -e "s|${PC_ROOT}|<root>|g" \
        -e "s|${HOME}/Library/Application Support/pico-8/carts|<oracle-carts>|g"
}

# Apply the per-cart mask (if any) to a normalised transcript.
# The mask file uses "cart|sed-expression" lines.
pc_apply_mask() {
    local mask_file="$PC_ROOT/tests/expected/masks.txt"
    local cart="$1"
    local expr=""
    local name e
    if [ -f "$mask_file" ]; then
        while IFS='|' read -r name e; do
            case "$name" in ''|\#*) continue ;; esac
            if [ "$name" = "$cart" ]; then expr="$e"; fi
        done < "$mask_file"
    fi
    if [ -n "$expr" ]; then
        sed -E "$expr"
    else
        cat
    fi
}
