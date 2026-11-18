#!/usr/bin/env python3
"""oracle.py — compare pico-c with the PICO-8 installed on Windows.

Usage:
    oracle.py [--bin PICO_C] [--record] [cart ...]

The Windows counterpart of platforms/macos/oracle.sh: every cmp_*.p8 cart of
tests/carts (or the carts given) runs headless in pico-c and with -x in the
real PICO-8, and the two transcripts are compared with the rules of the test
suite (tests/tools/run_tests.py: normalisation and masks.txt).

  MATCH  the transcripts are the same;
  KNOWN  the cart is listed in tests/expected/divergences.txt;
  ORDER  the same lines in another order.  Windows PICO-8 buffers its stdout
         when it is a pipe, so lines written there (errors, serial output,
         its own "RUNNING:") can come out after later printh lines, which
         go to stderr unbuffered.  This is not counted as a difference;
  DIFF   anything else (exit status 1).

--record rewrites tests/expected/<cart>.txt like oracle.sh --record does:
the oracle transcript for MATCH, the pico-c one for KNOWN.  ORDER carts keep
their golden file, since the order PICO-8 printed is lost.

PICO-8 runs with -home in a temporary folder that is removed afterwards, so
the user's config.txt and carts stay untouched.

Environment:
    PICO8        pico8.exe        (default C:\\Program Files (x86)\\PICO-8\\pico8.exe)
    PICO_C_BIN   the pico-c binary (default: the first one found in build/)
    ORACLE_WAIT  seconds before PICO-8 is stopped (default 8)

oracle.py — Windows에 설치된 PICO-8과 pico-c를 비교한다. platforms/macos/oracle.sh의
Windows판으로, tests/carts의 cmp_*.p8(또는 지정한 카트)를 pico-c(-headless)와
실제 PICO-8(-x)로 돌려 테스트 스위트와 같은 규칙(정규화, masks.txt)으로 비교한다.
결과는 MATCH(같음), KNOWN(divergences.txt에 있음), ORDER(같은 줄이 순서만 다름:
Windows PICO-8은 파이프로 받는 stdout을 버퍼링해서 오류/serial/자기 "RUNNING:" 줄이
stderr로 바로 나가는 printh보다 늦게 나올 수 있다. 차이로 치지 않음), DIFF(그 밖의
차이, 종료 코드 1). --record는 oracle.sh --record처럼 골든 파일을 다시 쓴다(MATCH는
오라클 출력, KNOWN은 pico-c 출력, ORDER는 원래 순서를 알 수 없으므로 그대로 둠).
PICO-8은 -home으로 임시 폴더에서 돌리고 끝나면 지우므로 사용자 config.txt와 카트는
건드리지 않는다.
"""
import collections
import difflib
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..', 'tests', 'tools')))
import run_tests as rt  # noqa: E402

PICO8 = os.environ.get('PICO8', r'C:\Program Files (x86)\PICO-8\pico8.exe')
WAIT = float(os.environ.get('ORACLE_WAIT', '8'))

# Files a cart reads from the folder it runs in; the oracle looks for them in
# its own carts folder.  카트가 자기 폴더에서 읽는 파일(오라클은 자기 카트 폴더에서 찾음).
SUPPORT_FILES = ['import_grad.png']


def divergences():
    """The carts of tests/expected/divergences.txt.
    tests/expected/divergences.txt에 적힌 카트들."""
    path = os.path.join(rt.EXPECTED, 'divergences.txt')
    if not os.path.isfile(path):
        return set()
    with open(path, encoding='latin-1') as f:
        return {l.strip() for l in f if l.strip() and not l.lstrip().startswith('#')}


def clean_paths(text, oracle_carts):
    """Replace the repository and the oracle's carts folder by placeholders,
    in both spellings (back and forward slashes).
    저장소와 오라클 카트 폴더 경로를 자리표시자로 바꾼다(\\와 / 두 표기 모두)."""
    for path, tag in ((rt.ROOT, '<root>'), (oracle_carts, '<oracle-carts>')):
        text = text.replace(path, tag).replace(rt.posix(path), tag)
    return text


def run_oracle(cart, home):
    """Run `cart` in PICO-8 with -x; return (transcript, killed).

    Both streams go into one pipe so lines keep the order PICO-8 wrote them
    in, as far as its buffering allows: printh goes to stderr with an "INFO: "
    prefix, which is removed; errors and serial output go to stdout.

    PICO-8에서 -x로 카트를 돌려 (트랜스크립트, 강제 종료 여부)를 돌려준다. 두 스트림을
    한 파이프로 받아 PICO-8이 쓴 순서를 (버퍼링이 허락하는 만큼) 지킨다. printh는
    stderr에 "INFO: "를 붙여 나오므로 떼어 내고, 오류와 serial 출력은 stdout으로 나온다.
    """
    carts = os.path.join(home, 'carts')
    p = subprocess.Popen([PICO8, '-home', home, '-x', rt.posix(cart)], cwd=carts,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        out, _ = p.communicate(timeout=WAIT)
        killed = False
    except subprocess.TimeoutExpired:
        p.kill()
        out, _ = p.communicate()
        killed = True
    lines = []
    for line in out.decode('latin-1').split('\n'):
        line = line.rstrip('\r')
        if line.startswith('INFO: '):
            line = line[len('INFO: '):]
        lines.append(line)
    return clean_paths('\n'.join(lines), carts), killed


def run_ours(binary, cart, oracle_carts):
    """Run `cart` headless in pico-c from tests/carts; return its transcript.
    tests/carts에서 pico-c로 카트를 헤드리스 실행해 트랜스크립트를 돌려준다."""
    p = subprocess.run([binary, '-headless', rt.posix(cart)], cwd=rt.CARTS,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    return clean_paths(p.stdout.decode('latin-1'), oracle_carts)


def record(name, lines):
    """Write tests/expected/<name>.txt with LF line endings.
    tests/expected/<name>.txt를 LF 줄바꿈으로 쓴다."""
    os.makedirs(rt.EXPECTED, exist_ok=True)
    with open(os.path.join(rt.EXPECTED, name + '.txt'), 'w', encoding='latin-1', newline='') as f:
        f.write(''.join(l + '\n' for l in lines))


def compare(binary, carts, home, recording):
    """Compare every cart; return the names that differ.
    모든 카트를 비교하고 다른 카트 이름들을 돌려준다."""
    oracle_carts = os.path.join(home, 'carts')
    masks = rt.load_masks()
    known = divergences()
    counts = collections.Counter()
    failed = []

    # A cart that writes a file another cart reads back must find it, so those
    # files are only removed once, at the beginning of the run.
    # 한 카트가 쓰고 다른 카트가 읽는 파일은 실행 시작 때 한 번만 지운다.
    rt.remove(rt.CARTS, rt.TRANSFER_FILES)

    for cart in carts:
        name = os.path.basename(cart)
        rt.remove(rt.CARTS, rt.STATE_FILES)
        rt.remove(oracle_carts, rt.STATE_FILES)
        ours = rt.normalize(run_ours(binary, cart, oracle_carts))
        text, killed = run_oracle(cart, home)
        theirs = rt.normalize(text)
        subs = masks.get(name, [])
        o = rt.apply_mask(ours, subs)
        t = rt.apply_mask(theirs, subs)

        if o == t:
            tag = 'MATCH'
            if recording:
                record(name, theirs)
        elif name in known:
            tag = 'KNOWN'
            if recording:
                record(name, ours)
        elif sorted(o) == sorted(t):
            tag = 'ORDER'
        else:
            tag = 'DIFF'
            failed.append(name)
        counts[tag] += 1

        note = '  (stopped after %gs)' % WAIT if killed else ''
        print('%-5s  %s%s' % (tag, name, note))
        if tag == 'DIFF':
            diff = difflib.unified_diff(t, o, 'oracle', 'pico-c', lineterm='', n=1)
            for line in list(diff)[2:42]:
                print('      ' + line)
        sys.stdout.flush()

    print('---- %d matched, %d known differences, %d in another order, %d differed'
          % (counts['MATCH'], counts['KNOWN'], counts['ORDER'], counts['DIFF']))
    if failed:
        print('differed: %s' % ' '.join(failed))
    return failed


def main(argv):
    binary = None
    recording = False
    carts = []
    while argv:
        arg = argv.pop(0)
        if arg == '--bin' and argv:
            binary = argv.pop(0)
        elif arg == '--record':
            recording = True
        elif arg in ('-h', '--help'):
            print(__doc__)
            return 0
        else:
            carts.append(arg)

    binary = os.path.abspath(rt.find_binary(binary))
    if not os.path.isfile(binary):
        sys.exit('error: pico-c binary not found: %s' % binary)
    if not os.path.isfile(PICO8):
        sys.exit('error: PICO-8 not found: %s (set PICO8)' % PICO8)

    if carts:
        carts = rt.transcript_carts(carts)
    else:
        carts = [c for c in rt.transcript_carts([]) if os.path.basename(c).startswith('cmp_')]

    home = tempfile.mkdtemp(prefix='pico-c-oracle-')
    try:
        os.makedirs(os.path.join(home, 'carts'))
        for name in SUPPORT_FILES:
            shutil.copy(os.path.join(rt.CARTS, name), os.path.join(home, 'carts'))
        failed = compare(binary, carts, home, recording)
    finally:
        shutil.rmtree(home, ignore_errors=True)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
