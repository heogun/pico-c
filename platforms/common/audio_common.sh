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

# Every line of an expected file is one of:
#   <sfx> <window> <count> | <notes>          the notes, compared exactly
#   <sfx> <feature> [arg]  | <value> ~<tol>   a feature (rms, harm K, peak R,
#                                             hold), compared within <tol>
# 기대 파일의 줄은 음 판독(정확히 비교) 아니면 특징값(허용 오차 안에서 비교)이다.

# pc_audio_read <dir> <name> <sfx> <args...> — what wavnotes.py reads.
pc_audio_read() {
    local dir="$1" name="$2" sfx="$3"
    shift 3
    "$(pc_audio_python)" -I "$PC_WAVNOTES" "$dir/${name}_$sfx.wav" "$@" 2>/dev/null
}

# pc_audio_check <name> <wav-dir> — compare the readouts of a folder of
# .wav files with the expected file.  Prints a line per check and returns
# the number of mismatches.
pc_audio_check() {
    local name="$1" dir="$2" fail=0 line left right sfx args want tol got ok
    while IFS= read -r line; do
        case "$line" in ''|\#*) continue ;; esac
        left="${line%%|*}"
        right="${line#*|}"
        read -r sfx args <<< "$left"
        right="$(printf '%s' "$right" | sed 's/^ *//')"
        # shellcheck disable=SC2086
        got="$(pc_audio_read "$dir" "$name" "$sfx" $args)"
        case "$right" in
            *'~'*)
                want="${right%%~*}"; tol="${right#*~}"
                ok="$(awk -v g="$got" -v w="$want" -v t="$tol" \
                      'BEGIN { d = g - w; if (d < 0) d = -d; print (g != "" && d <= t + 1e-9) ? 1 : 0 }')"
                ;;
            *)
                want="$right"
                [ "$got" = "$want" ] && ok=1 || ok=0
                ;;
        esac
        if [ "$ok" = 1 ]; then
            printf 'PASS  %s sfx %s %s\n' "$name" "$sfx" "$args"
        else
            printf 'FAIL  %s sfx %s %s\n      want %s\n      got  %s\n' \
                "$name" "$sfx" "$args" "$right" "$got"
            fail=$((fail + 1))
        fi
    done < "$PC_AUDIO_DIR/expected/$name.txt"
    return $fail
}

# pc_audio_record <name> <wav-dir> — rewrite the expected file from a folder
# of .wav files.  The checks and the tolerances stay as they are; only the
# notes and the values are replaced.
pc_audio_record() {
    local name="$1" dir="$2" exp tmp line left right sfx args tol got
    exp="$PC_AUDIO_DIR/expected/$name.txt"
    tmp="$exp.tmp"
    : > "$tmp"
    while IFS= read -r line; do
        case "$line" in
            ''|\#*) printf '%s\n' "$line" >> "$tmp"; continue ;;
        esac
        left="${line%%|*}"
        right="${line#*|}"
        read -r sfx args <<< "$left"
        # shellcheck disable=SC2086
        got="$(pc_audio_read "$dir" "$name" "$sfx" $args)"
        case "$right" in
            *'~'*) tol="${right#*~}"; printf '%s| %s ~%s\n' "$left" "$got" "$tol" >> "$tmp" ;;
            *)     printf '%s| %s\n' "$left" "$got" >> "$tmp" ;;
        esac
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
