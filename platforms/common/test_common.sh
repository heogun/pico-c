#!/usr/bin/env bash
# pico-c regression suite.
#
#   test_common.sh <platform> [cart ...]
#
# Runs every cart in tests/carts that has a golden transcript in
# tests/expected, in headless mode, and compares the output with the golden
# file.  The golden transcripts were produced by the real PICO-8 (see
# platforms/macos/oracle.sh), so this suite checks pico-c against PICO-8 on
# every platform, not just on the machine that has PICO-8 installed.
#
# Environment:
#   PICO_C_BIN   path of the pico-c binary (default build/<platform>/pico-c)

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

CARTS_DIR="$(pc_carts_dir)"
EXPECTED_DIR="$(pc_expected_dir)"

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

# ---------------------------------------------------------------- cart list --
# The suite is made of the carts that have a golden transcript.  A cart that
# is named cmp_* but has no golden transcript is reported, it should have one.
is_transfer_file() {
    local f="$1" t
    for t in "${PC_TRANSFER_FILES[@]}"; do
        [ "$f" = "$t" ] && return 0
    done
    return 1
}

if [ $# -gt 0 ]; then
    CARTS=()
    for c in "$@"; do
        case "$c" in
            /*) CARTS+=("$c") ;;
            *.p8) CARTS+=("$CARTS_DIR/$c") ;;
            *)   CARTS+=("$CARTS_DIR/$c.p8") ;;
        esac
    done
else
    CARTS=()
    while IFS= read -r f; do
        name="$(basename "$f")"
        is_transfer_file "$name" && continue
        if [ -f "$EXPECTED_DIR/$name.txt" ] || [ "${name#cmp_}" != "$name" ]; then
            CARTS+=("$f")
        fi
    done < <(ls "$CARTS_DIR"/*.p8 2>/dev/null | sort)
fi

if [ ${#CARTS[@]} -eq 0 ]; then
    pc_die "no carts found in $CARTS_DIR"
fi

# -------------------------------------------------------------------- run ----
# A cart that writes a file another cart reads back must find it, so those
# files are only removed once, at the beginning of the run.
pc_clean_files "$CARTS_DIR" "${PC_TRANSFER_FILES[@]}"

pass=0
fail=0
skip=0
failed=()

for cart in "${CARTS[@]}"; do
    name="$(basename "$cart")"
    golden="$EXPECTED_DIR/$name.txt"

    if [ ! -f "$golden" ]; then
        printf 'SKIP  %s (no golden file)\n' "$name"
        skip=$((skip + 1))
        continue
    fi

    # State left over by a previous cart would change the result.
    pc_clean_files "$CARTS_DIR" "${PC_STATE_FILES[@]}"

    tmp="$(mktemp "${TMPDIR:-/tmp}/pico-c-test.XXXXXX")"
    # The cart is run from tests/carts so that carts reading or writing a
    # sibling file (cstore_base.p8, loadB.p8, ...) find it.
    ( cd "$CARTS_DIR" && "$BIN" -headless "$cart" ) > "$tmp" 2>&1
    pc_normalize < "$tmp" | pc_apply_mask "$name" > "$tmp.norm"
    pc_apply_mask "$name" < "$golden" > "$tmp.golden"

    if diff -u "$tmp.golden" "$tmp.norm" > "$tmp.diff"; then
        printf 'PASS  %s\n' "$name"
        pass=$((pass + 1))
    else
        printf 'FAIL  %s\n' "$name"
        fail=$((fail + 1))
        failed+=("$name")
        sed -n '1,40p' "$tmp.diff" | sed 's/^/      /'
    fi
    rm -f "$tmp" "$tmp.norm" "$tmp.golden" "$tmp.diff"
done

# The files a cart wrote for another cart are not part of the repository.
pc_clean_files "$CARTS_DIR" "${PC_TRANSFER_FILES[@]}"

printf -- '---- %d passed, %d failed, %d skipped\n' "$pass" "$fail" "$skip"
if [ $fail -gt 0 ]; then
    printf 'failed: %s\n' "${failed[*]}"
    exit 1
fi
exit 0
