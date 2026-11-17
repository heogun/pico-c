#!/usr/bin/env bash
# pico-c audio suite helpers for platforms/macos/oracle_audio.sh.
#
# The checks themselves live in tests/tools/run_tests.py (audio-check,
# audio-record); these functions only call it, so that pico-c's test run and
# the recording from PICO-8 read the .wav files the same way.
#
# oracle_audio.sh용 오디오 스위트 헬퍼. 검사 자체는 tests/tools/run_tests.py
# (audio-check, audio-record)에 있고, 이 함수들은 그것을 부르기만 한다.

# shellcheck source=common.sh
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

PC_AUDIO_DIR="$PC_ROOT/tests/audio"
PC_RUN_TESTS="$PC_ROOT/tests/tools/run_tests.py"

# pc_audio_python — the python interpreter, or nothing when there is none.
pc_audio_python() {
    command -v python3 2>/dev/null || command -v python 2>/dev/null || true
}

# pc_audio_check <name> <wav-dir> — compare a folder of .wav files with the
# expected file; prints a line per check, non-zero when one fails.
pc_audio_check() {
    "$(pc_audio_python)" "$PC_RUN_TESTS" audio-check "$1" "$2"
}

# pc_audio_record <name> <wav-dir> — rewrite the expected file from a folder
# of .wav files (the checks and the tolerances stay as they are).
pc_audio_record() {
    "$(pc_audio_python)" "$PC_RUN_TESTS" audio-record "$1" "$2"
}
