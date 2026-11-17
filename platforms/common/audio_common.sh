#!/usr/bin/env bash
# pico-c audio suite.
#
#   audio_common.sh <binary> <wav-dir-of-a-reference>?
#
# Shared by platforms/common/test_common.sh (which checks pico-c) and
# platforms/macos/oracle_audio.sh (which records PICO-8).  Every cart in
# tests/audio/carts exports its sound effects as <name>_<n>.wav; every line
# of tests/audio/expected/<name>.txt names a sound effect, a window and a
# count, and the notes that tests/tools/wavnotes.py reads from that file.
#
# pico-c 오디오 스위트. tests/audio/carts의 카트는 효과음을 <name>_<n>.wav로
# 내보내고, tests/audio/expected/<name>.txt의 줄마다 효과음·창·개수와
# tests/tools/wavnotes.py가 그 파일에서 읽어 낸 음이 적혀 있다.

# shellcheck source=common.sh
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

PC_AUDIO_DIR="$PC_ROOT/tests/audio"
PC_WAVNOTES="$PC_ROOT/tests/tools/wavnotes.py"

# pc_audio_python — the python interpreter, or nothing when there is none.
pc_audio_python() {
    command -v python3 2>/dev/null || command -v python 2>/dev/null || true
}

# pc_audio_check <name> <wav-dir> — compare the readouts of a folder of
# .wav files with the expected file.  Prints a line per sound effect and
# returns the number of mismatches.
pc_audio_check() {
    local name="$1" dir="$2" py fail=0 sfx win cnt bar want got
    py="$(pc_audio_python)"
    while read -r sfx win cnt bar want; do
        case "$sfx" in ''|\#*) continue ;; esac
        got="$("$py" -I "$PC_WAVNOTES" "$dir/${name}_$sfx.wav" "$win" "$cnt" 2>/dev/null)"
        if [ "$got" = "$want" ]; then
            printf 'PASS  %s sfx %s\n' "$name" "$sfx"
        else
            printf 'FAIL  %s sfx %s\n      want %s\n      got  %s\n' "$name" "$sfx" "$want" "$got"
            fail=$((fail + 1))
        fi
    done < "$PC_AUDIO_DIR/expected/$name.txt"
    return $fail
}

# pc_audio_record <name> <wav-dir> — rewrite the expected file from a folder
# of .wav files (the sound effects, windows and counts stay as they are).
pc_audio_record() {
    local name="$1" dir="$2" py exp tmp sfx win cnt rest
    py="$(pc_audio_python)"
    exp="$PC_AUDIO_DIR/expected/$name.txt"
    tmp="$exp.tmp"
    : > "$tmp"
    while IFS= read -r line; do
        case "$line" in
            ''|\#*) printf '%s\n' "$line" >> "$tmp"; continue ;;
        esac
        read -r sfx win cnt rest <<< "$line"
        printf '%s %s %s | %s\n' "$sfx" "$win" "$cnt" \
            "$("$py" -I "$PC_WAVNOTES" "$dir/${name}_$sfx.wav" "$win" "$cnt")" >> "$tmp"
    done < "$exp"
    mv "$tmp" "$exp"
}

# pc_audio_suite <binary> — run every audio cart with pico-c and check it.
# Returns the number of failures (0 when python is missing: then the suite
# is skipped with a note).
pc_audio_suite() {
    local bin="$1" fails=0 cart name work
    if [ -z "$(pc_audio_python)" ]; then
        printf 'SKIP  audio suite (python3 not found)\n'
        return 0
    fi
    for cart in "$PC_AUDIO_DIR"/carts/*.p8; do
        name="$(basename "$cart" .p8)"
        [ -f "$PC_AUDIO_DIR/expected/$name.txt" ] || continue
        work="$(mktemp -d "${TMPDIR:-/tmp}/pico-c-audio.XXXXXX")"
        cp "$cart" "$work/"
        ( cd "$work" && "$bin" -headless "$name.p8" ) > /dev/null 2>&1
        pc_audio_check "$name" "$work"
        fails=$((fails + $?))
        rm -f "$work"/*.wav "$work/$name.p8"
        rmdir "$work"
    done
    return $fails
}
