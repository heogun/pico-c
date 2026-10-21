#!/usr/bin/env bash
# One-off helper: split src/api.cpp into the per-subsystem modules of src/api/.
#
# The original api.cpp was a single 2251 line file with every Lua function of
# the PICO-8 API in it.  It is split into one module per subsystem, each with
# its own registration table, so that a change to (say) the drawing code does
# not have to touch the cartridge code.
#
# The ranges below are the line ranges of the original file; the script is
# kept so the split can be reviewed and repeated.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
SRC=src/api.cpp
DST=src/api
mkdir -p "$DST"

# extract <file> <start> <end> ...   -- appends the line ranges to <file>
extract() {
    local out="$1"; shift
    while [ $# -ge 2 ]; do
        sed -n "$1,$2p" "$SRC" >> "$out"
        printf '\n' >> "$out"
        shift 2
    done
}

# --------------------------------------------------------------------- gfx --
: > "$DST/api_gfx.cpp"
extract "$DST/api_gfx.cpp" \
    12   12 \
    83   89 \
    148  339 \
    341  404 \
    637  673 \
    675  841 \
    843  1038 \
    1266 1485

# -------------------------------------------------------------------- text --
: > "$DST/api_text.cpp"
extract "$DST/api_text.cpp" \
    406  419 \
    421  447 \
    449  454 \
    456  586 \
    588  635 \
    1733 1779 \
    1910 1916 \
    1918 1978

# ------------------------------------------------------------------ memory --
: > "$DST/api_memory.cpp"
extract "$DST/api_memory.cpp" \
    1040 1141

# ------------------------------------------------------------------- input --
: > "$DST/api_input.cpp"
extract "$DST/api_input.cpp" \
    91  146

# ------------------------------------------------------------------- audio --
: > "$DST/api_audio.cpp"
extract "$DST/api_audio.cpp" \
    1831 1854

# ------------------------------------------------------------------ system --
: > "$DST/api_system.cpp"
extract "$DST/api_system.cpp" \
    1143 1147 \
    1178 1264 \
    1487 1583 \
    1585 1731

# -------------------------------------------------------------------- cart --
: > "$DST/api_cart.cpp"
extract "$DST/api_cart.cpp" \
    1855 1909 \
    1980 2068

# --------------------------------------------------------------------- lua --
: > "$DST/api_lua.cpp"
extract "$DST/api_lua.cpp" \
    1149 1176 \
    1781 1829 \
    2070 2180

echo "split done"
