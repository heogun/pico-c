#!/usr/bin/env bash
# macOS only: compare pico-c against the real PICO-8 (the "oracle") and
# refresh the golden transcripts used by the portable test suite.
#
#   oracle.sh                compare every cart of tests/carts
#   oracle.sh --record       rewrite tests/expected/<cart>.txt
#   oracle.sh cart.p8 ...    restrict the run to the given carts
#
# The oracle is started from a signed copy in /tmp/p8app: /Applications/PICO-8.app
# hangs on some machines and is code signed by hand.
#
# Environment:
#   PICO8_APP    the PICO-8 bundle            (default /Applications/PICO-8.app)
#   ORACLE_DIR   the signed copy to run       (default /tmp/p8app)
#   ORACLE_WAIT  seconds to let the cart run  (default 6)

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../common/common.sh
. "$SCRIPT_DIR/../common/common.sh"

PLATFORM=macos
CARTS_DIR="$(pc_carts_dir)"
EXPECTED_DIR="$(pc_expected_dir)"
DIVERGENCES="$EXPECTED_DIR/divergences.txt"

PICO8_APP="${PICO8_APP:-/Applications/PICO-8.app}"
ORACLE_DIR="${ORACLE_DIR:-/tmp/p8app}"
ORACLE_WAIT="${ORACLE_WAIT:-6}"
ORACLE_CWD="$HOME/Library/Application Support/pico-8/carts"

record=0
list=()
for arg in "$@"; do
    case "$arg" in
        --record) record=1 ;;
        *) list+=("$arg") ;;
    esac
done

# ------------------------------------------------------------------ oracle --
prepare_oracle() {
    if [ -x "$ORACLE_DIR/Contents/MacOS/pico8" ]; then
        return 0
    fi
    [ -d "$PICO8_APP" ] || pc_die "PICO-8 not found at $PICO8_APP"
    printf 'preparing a signed copy of PICO-8 in %s ...\n' "$ORACLE_DIR"
    rm -rf "$ORACLE_DIR"
    cp -R "$PICO8_APP" "$ORACLE_DIR"
    # The bundle contains an SDL2 framework; signing it separately keeps
    # codesign from refusing the whole bundle.
    codesign --force --sign - "$ORACLE_DIR/Contents/Frameworks/SDL2.framework" >/dev/null 2>&1 || true
    codesign --force --deep --sign - "$ORACLE_DIR" >/dev/null 2>&1 || true
    codesign --force --sign - "$ORACLE_DIR/Contents/MacOS/pico8" >/dev/null 2>&1 || true
    [ -x "$ORACLE_DIR/Contents/MacOS/pico8" ] || pc_die "could not prepare the oracle in $ORACLE_DIR"
}

run_oracle() {
    local cart="$1" out="$2"
    ( "$ORACLE_DIR/Contents/MacOS/pico8" -x "$cart" > "$out" 2>&1 & )
    sleep "$ORACLE_WAIT"
    pkill -f p8app >/dev/null 2>&1
}

# Is the cart known to differ from the oracle on purpose?
is_divergence() {
    local name="$1"
    [ -f "$DIVERGENCES" ] || return 1
    grep -q "^[[:space:]]*${name}[[:space:]]*$" "$DIVERGENCES"
}

# ------------------------------------------------------------------- carts --
if [ ${#list[@]} -gt 0 ]; then
    CARTS=()
    for c in "${list[@]}"; do
        case "$c" in
            /*) CARTS+=("$c") ;;
            *.p8) CARTS+=("$CARTS_DIR/$c") ;;
            *)   CARTS+=("$CARTS_DIR/$c.p8") ;;
        esac
    done
else
    CARTS=()
    while IFS= read -r f; do CARTS+=("$f"); done < <(ls "$CARTS_DIR"/cmp_*.p8 2>/dev/null | sort)
fi

BIN="${PICO_C_BIN:-$PC_ROOT/build/$PLATFORM/pico-c}"
if [ ! -x "$BIN" ]; then
    pc_die "pico-c binary not found: $BIN
Build it first:  make PLATFORM=$PLATFORM"
fi

prepare_oracle

# A cart that writes a file another cart reads back must find it, so those
# files are only removed once, at the beginning of the run.
pc_clean_files "$CARTS_DIR" "${PC_TRANSFER_FILES[@]}"
pc_clean_files "$ORACLE_CWD" "${PC_TRANSFER_FILES[@]}"

pass=0
fail=0
diverged=0
failed=()

for cart in "${CARTS[@]}"; do
    name="$(basename "$cart")"
    abs="$CARTS_DIR/$name"

    ours="$(mktemp "${TMPDIR:-/tmp}/pico-c-ours.XXXXXX")"
    theirs="$(mktemp "${TMPDIR:-/tmp}/pico-c-oracle.XXXXXX")"

    # State left over by a previous cart would change the result.
    pc_clean_files "$CARTS_DIR" "${PC_STATE_FILES[@]}"
    pc_clean_files "$ORACLE_CWD" "${PC_STATE_FILES[@]}"

    ( cd "$CARTS_DIR" && "$BIN" -headless "$abs" ) > "$ours" 2>&1
    run_oracle "$abs" "$theirs"

    # The golden transcripts are stored unmasked; the mask is applied to both
    # sides when they are compared.
    pc_normalize < "$ours"   > "$ours.norm"
    pc_normalize < "$theirs" > "$theirs.norm"
    pc_apply_mask "$name" < "$ours.norm"   > "$ours.masked"
    pc_apply_mask "$name" < "$theirs.norm" > "$theirs.masked"

    if diff -u "$theirs.masked" "$ours.masked" > /tmp/pico-c-oracle.diff; then
        printf 'MATCH  %s\n' "$name"
        pass=$((pass + 1))
        if [ $record -eq 1 ]; then
            mkdir -p "$EXPECTED_DIR"
            cp "$theirs.norm" "$EXPECTED_DIR/$name.txt"
        fi
    elif is_divergence "$name"; then
        # The oracle cannot do what the cart asks for (PICO-8 disables
        # save() under -x); the golden transcript is the one of pico-c.
        printf 'KNOWN %s (differs from the oracle on purpose)\n' "$name"
        diverged=$((diverged + 1))
        if [ $record -eq 1 ]; then
            mkdir -p "$EXPECTED_DIR"
            cp "$ours.norm" "$EXPECTED_DIR/$name.txt"
        fi
    else
        printf 'DIFF   %s\n' "$name"
        fail=$((fail + 1))
        failed+=("$name")
        sed -n '1,40p' /tmp/pico-c-oracle.diff | sed 's/^/      /'
    fi

    rm -f "$ours" "$theirs" "$ours.norm" "$theirs.norm" "$ours.masked" "$theirs.masked"
done

printf -- '---- %d matched, %d known differences, %d differed\n' "$pass" "$diverged" "$fail"
if [ $fail -gt 0 ]; then
    printf 'differed: %s\n' "${failed[*]}"
    exit 1
fi
exit 0
