#!/usr/bin/env bash
# oracle_audio.sh — check the audio suite against the real PICO-8.
#
#   platforms/macos/oracle_audio.sh            compare PICO-8 with tests/audio/expected
#   platforms/macos/oracle_audio.sh --record   rewrite tests/audio/expected from PICO-8
#
# PICO-8 only renders sound in a windowed run, so a window opens for a few
# seconds per cart.  It runs with its own home folder (-home), so the
# user's PICO-8 settings and carts are not touched: the exported .wav files
# land in that folder.  A personal config with "sound_volume 0" would
# otherwise export silence.
#
# PICO-8은 창 모드에서만 소리를 만들므로 카트마다 창이 몇 초 열린다. 자기 홈
# 폴더(-home)로 돌리므로 사용자의 PICO-8 설정과 카트는 건드리지 않는다. 개인
# 설정에 "sound_volume 0"이 있으면 무음이 나오기 때문이기도 하다.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$SCRIPT_DIR/../common/audio_common.sh"

PICO8_APP="${PICO8_APP:-/Applications/PICO-8.app}"
ORACLE_DIR="${ORACLE_DIR:-/tmp/p8app}"
ORACLE_WAIT="${ORACLE_WAIT:-40}"

record=0
[ "${1:-}" = "--record" ] && record=1

if [ ! -x "$ORACLE_DIR/Contents/MacOS/pico8" ]; then
    [ -d "$PICO8_APP" ] || pc_die "PICO-8 not found at $PICO8_APP"
    printf 'preparing a signed copy of PICO-8 in %s ...\n' "$ORACLE_DIR"
    cp -R "$PICO8_APP" "$ORACLE_DIR"
    codesign --force --sign - "$ORACLE_DIR/Contents/Frameworks/SDL2.framework" >/dev/null 2>&1 || true
    codesign --force --deep --sign - "$ORACLE_DIR" >/dev/null 2>&1 || true
    codesign --force --sign - "$ORACLE_DIR/Contents/MacOS/pico8" >/dev/null 2>&1 || true
fi
[ -n "$(pc_audio_python)" ] || pc_die "python3 is needed to read the .wav files"

fails=0
for cart in "$PC_AUDIO_DIR"/carts/*.p8; do
    name="$(basename "$cart" .p8)"
    [ -f "$PC_AUDIO_DIR/expected/$name.txt" ] || continue
    home="$(mktemp -d "${TMPDIR:-/tmp}/pico8-home.XXXXXX")"
    ( "$ORACLE_DIR/Contents/MacOS/pico8" -home "$home" -windowed 1 -run "$cart" \
        > /dev/null 2>&1 & )
    for _ in $(seq 1 "$ORACLE_WAIT"); do
        sleep 1
        [ -f "$home/carts/${name}_63.wav" ] && break
    done
    sleep 1
    pkill -f "$ORACLE_DIR/Contents/MacOS/pico8"
    [ -f "$home/carts/${name}_63.wav" ] || pc_die "PICO-8 did not export $name"

    if [ $record -eq 1 ]; then
        pc_audio_record "$name" "$home/carts"
        printf 'recorded %s\n' "$PC_AUDIO_DIR/expected/$name.txt"
    else
        pc_audio_check "$name" "$home/carts"
        fails=$((fails + $?))
    fi
    case "$home" in "${TMPDIR:-/tmp}"/pico8-home.*) rm -rf "$home" ;; esac
done

[ $fails -eq 0 ]
