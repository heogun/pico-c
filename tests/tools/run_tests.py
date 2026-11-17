#!/usr/bin/env python3
"""run_tests.py — the pico-c regression suite, on every platform.

Usage:
    run_tests.py [--bin PICO_C] [cart ...]      run the suite (or some carts)
    run_tests.py audio-check NAME WAV_DIR       compare a folder of .wav files
    run_tests.py audio-record NAME WAV_DIR      rewrite tests/audio/expected/NAME.txt

The suite has two parts:

  * transcripts: every cart in tests/carts with a golden file in
    tests/expected runs headless, and what it prints is compared with the
    golden file (recorded from the real PICO-8);
  * audio (only for the whole suite): every cart in tests/audio/carts
    exports its sound as .wav files, which tests/tools/wavnotes.py reads
    back and compares with tests/audio/expected/<name>.txt.

The binary is --bin, $PICO_C_BIN, or the first one found in build/.
The platform scripts (platforms/*/test.sh, test.bat) call this file; it
needs nothing but Python 3, so it runs the same on macOS, Linux and
Windows.  Line endings are normalised, so a Windows binary that writes
"\\r\\n" compares equal.

run_tests.py — 모든 플랫폼에서 도는 pico-c 회귀 스위트. 트랜스크립트(골든 파일과
출력 비교)와 오디오(.wav를 wavnotes.py로 판독해 기대값과 비교) 두 부분이다.
바이너리는 --bin, $PICO_C_BIN, 아니면 build/에서 처음 찾은 것. 플랫폼 스크립트가
이 파일을 부르며 Python 3만 있으면 되므로 macOS, Linux, Windows에서 똑같이 돈다.
줄바꿈은 정규화되므로 "\\r\\n"을 쓰는 Windows 바이너리도 같게 비교된다.
"""
import difflib
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
CARTS = os.path.join(ROOT, 'tests', 'carts')
EXPECTED = os.path.join(ROOT, 'tests', 'expected')
AUDIO = os.path.join(ROOT, 'tests', 'audio')

# Files a cart writes and the next run must not find.  다음 실행이 보면 안 되는 파일.
STATE_FILES = ['guard.p8', 'ptest.txt', 'runtest2.txt']
# Files one cart writes for another to read back.  한 카트가 다른 카트를 위해 쓰는 파일.
TRANSFER_FILES = ['p8save_out.p8', 'p8png_out.p8.png']

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wavnotes  # noqa: E402


def posix(path):
    return path.replace('\\', '/')


def find_binary(given):
    """The pico-c binary to test.  테스트할 pico-c 바이너리."""
    if given:
        return given
    if os.environ.get('PICO_C_BIN'):
        return os.environ['PICO_C_BIN']
    candidates = []
    for sub in ('macos', 'linux', 'windows', 'cmake', 'msvc'):
        for rel in ('pico-c', 'pico-c.exe', 'Release/pico-c.exe', 'RelWithDebInfo/pico-c.exe',
                    'Debug/pico-c.exe'):
            candidates.append(os.path.join(ROOT, 'build', sub, rel))
    for c in candidates:
        if os.path.isfile(c):
            return c
    sys.exit('error: pico-c binary not found; build it first or pass --bin')


def remove(folder, names):
    for n in names:
        try:
            os.remove(os.path.join(folder, n))
        except OSError:
            pass


# ----------------------------------------------------------- transcripts --

def normalize(text):
    """Make a transcript comparable: the same rules as before (common.sh).
    트랜스크립트를 비교할 수 있게 만든다(예전 common.sh와 같은 규칙)."""
    root = posix(ROOT)
    home = posix(os.path.expanduser('~'))
    out = []
    for line in text.split('\n'):
        line = line.rstrip('\r')
        if line.startswith('RUNNING:') or line.startswith('DEBUG'):
            continue
        if not line.strip():
            continue
        line = line.replace(root, '<root>')
        line = line.replace(home + '/Library/Application Support/pico-8/carts', '<oracle-carts>')
        out.append(line)
    return out


def load_masks():
    """tests/expected/masks.txt: "cart|s/re/rep/g; s/re/rep/g" lines."""
    masks = {}
    path = os.path.join(EXPECTED, 'masks.txt')
    if not os.path.isfile(path):
        return masks
    with open(path, encoding='latin-1') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line or line.startswith('#') or '|' not in line:
                continue
            name, expr = line.split('|', 1)
            subs = []
            for part in expr.split(';'):
                part = part.strip()
                m = re.match(r'^s/(.*)/(.*)/(g?)$', part)
                if m:
                    subs.append((re.compile(m.group(1)), m.group(2), m.group(3) == 'g'))
            masks[name] = subs
    return masks


def apply_mask(lines, subs):
    out = []
    for line in lines:
        for regex, rep, every in subs:
            line = regex.sub(rep, line, count=0 if every else 1)
        out.append(line)
    return out


def transcript_carts(args):
    if args:
        carts = []
        for c in args:
            if os.path.isabs(c):
                carts.append(c)
            else:
                carts.append(os.path.join(CARTS, c if c.endswith('.p8') else c + '.p8'))
        return carts
    carts = []
    for name in sorted(os.listdir(CARTS)):
        if not name.endswith('.p8') or name in TRANSFER_FILES:
            continue
        if os.path.isfile(os.path.join(EXPECTED, name + '.txt')) or name.startswith('cmp_'):
            carts.append(os.path.join(CARTS, name))
    return carts


def run_transcripts(binary, carts):
    masks = load_masks()
    passed = failed = skipped = 0
    failures = []
    remove(CARTS, TRANSFER_FILES)
    for cart in carts:
        name = os.path.basename(cart)
        golden_path = os.path.join(EXPECTED, name + '.txt')
        if not os.path.isfile(golden_path):
            print('SKIP  %s (no golden file)' % name)
            skipped += 1
            continue
        remove(CARTS, STATE_FILES)
        proc = subprocess.run([binary, '-headless', posix(cart)], cwd=CARTS,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              stdin=subprocess.DEVNULL)
        got = normalize(proc.stdout.decode('latin-1'))
        with open(golden_path, 'rb') as f:
            want = f.read().decode('latin-1').split('\n')
        want = [l.rstrip('\r') for l in want]
        subs = masks.get(name, [])
        got, want = apply_mask(got, subs), apply_mask([l for l in want if l.strip()], subs)
        if got == want:
            print('PASS  %s' % name)
            passed += 1
        else:
            print('FAIL  %s' % name)
            diff = list(difflib.unified_diff(want, got, 'expected', 'pico-c', lineterm=''))
            for line in diff[:40]:
                print('      ' + line)
            failed += 1
            failures.append(name)
    remove(CARTS, TRANSFER_FILES)
    return passed, failed, skipped, failures


# ------------------------------------------------------------------ audio --

def readout(wav_dir, name, sfx, args):
    """What wavnotes.py reads for one check.  검사 하나에 대해 wavnotes.py가 읽는 값."""
    path = os.path.join(wav_dir, '%s_%s.wav' % (name, sfx))
    try:
        samples = wavnotes.read(path)
    except (OSError, EOFError, Exception):  # a missing or broken file
        return ''
    if args and args[0].isdigit():
        return wavnotes.notes(samples, int(args[0]), int(args[1]))
    arg = float(args[1]) if len(args) > 1 else 0.0
    return '%.3f' % wavnotes.feature(samples, args[0], arg)


def parse_check(line):
    left, right = line.split('|', 1)
    fields = left.split()
    return fields[0], fields[1:], right.strip()


def audio_check(name, wav_dir):
    """Compare a folder of .wav files with tests/audio/expected/NAME.txt.
    Prints a line per check; returns (passed, failed).
    .wav 폴더를 기대 파일과 비교한다. 검사마다 한 줄을 찍고 (통과, 실패)를 돌려준다."""
    passed = failed = 0
    with open(os.path.join(AUDIO, 'expected', name + '.txt'), encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line or line.startswith('#'):
                continue
            sfx, args, want = parse_check(line)
            got = readout(wav_dir, name, sfx, args)
            if '~' in want:
                value, tol = want.split('~')
                ok = got != '' and abs(float(got) - float(value)) <= float(tol) + 1e-9
            else:
                ok = got == want
            label = '%s sfx %s %s' % (name, sfx, ' '.join(args))
            if ok:
                print('PASS  ' + label)
                passed += 1
            else:
                print('FAIL  %s\n      want %s\n      got  %s' % (label, want, got))
                failed += 1
    return passed, failed


def audio_record(name, wav_dir):
    """Rewrite the expected file from a folder of .wav files, keeping the checks
    and the tolerances.  검사와 허용 오차는 그대로 두고 값만 다시 쓴다."""
    path = os.path.join(AUDIO, 'expected', name + '.txt')
    out = []
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line or line.startswith('#'):
                out.append(line)
                continue
            left = line.split('|', 1)[0]
            sfx, args, want = parse_check(line)
            got = readout(wav_dir, name, sfx, args)
            if '~' in want:
                out.append('%s| %s ~%s' % (left, got, want.split('~')[1]))
            else:
                out.append('%s| %s' % (left, got))
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out) + '\n')


def run_audio(binary):
    passed = failed = 0
    carts_dir = os.path.join(AUDIO, 'carts')
    for cart in sorted(os.listdir(carts_dir)):
        if not cart.endswith('.p8'):
            continue
        name = cart[:-3]
        if not os.path.isfile(os.path.join(AUDIO, 'expected', name + '.txt')):
            continue
        work = tempfile.mkdtemp(prefix='pico-c-audio-')
        try:
            shutil.copy(os.path.join(carts_dir, cart), work)
            subprocess.run([binary, '-headless', cart], cwd=work,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           stdin=subprocess.DEVNULL)
            p, f = audio_check(name, work)
            passed += p
            failed += f
        finally:
            shutil.rmtree(work, ignore_errors=True)
    return passed, failed


# ------------------------------------------------------------------- main --

def main(argv):
    if argv and argv[0] in ('audio-check', 'audio-record'):
        if len(argv) != 3:
            sys.exit(__doc__)
        if argv[0] == 'audio-check':
            _, failed = audio_check(argv[1], argv[2])
            return 1 if failed else 0
        audio_record(argv[1], argv[2])
        return 0

    binary = None
    if len(argv) >= 2 and argv[0] == '--bin':
        binary, argv = argv[1], argv[2:]
    binary = os.path.abspath(find_binary(binary))
    if not os.path.isfile(binary):
        sys.exit('error: pico-c binary not found: %s' % binary)

    carts = transcript_carts(argv)
    if not carts:
        sys.exit('error: no carts found in %s' % CARTS)
    passed, failed, skipped, failures = run_transcripts(binary, carts)

    if not argv:                      # the audio suite runs with the whole suite
        ap, af = run_audio(binary)
        passed += ap
        failed += af
        if af:
            failures.append('audio')

    print('---- %d passed, %d failed, %d skipped' % (passed, failed, skipped))
    if failures:
        print('failed: %s' % ' '.join(failures))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
