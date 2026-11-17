#!/usr/bin/env python3
"""wavnotes.py — read the notes back out of a .wav file.

Usage: wavnotes.py FILE.wav WINDOW COUNT     the notes, window by window
       wavnotes.py FILE.wav rms [END]       loudness of the steady part (up to
                                            sample END instead of 2600)
       wavnotes.py FILE.wav harm K          K-th harmonic / fundamental
       wavnotes.py FILE.wav peak RATIO      spectrum at RATIO * fundamental
                                            / fundamental (a second voice)
       wavnotes.py FILE.wav hold            median length of the flat runs of
                                            3 samples or more (noiz noise)

The features (all but the first form) read samples 600..2600 of a note at
key 24, the fundamental being 261.63Hz; they print one number, which the
audio suite compares with a tolerance.

Cuts the sound into COUNT windows of WINDOW samples (22050Hz) and prints,
for every window, the pitch it holds as a key relative to key 24 (C-2 in
PICO-8's tracker), or "." when it is silent.  The first SKIP samples of a
window are left out, because PICO-8's own export starts every change a
little late (up to one tick of 183 samples); reading the later part of the
window makes the output of PICO-8 and of pico-c comparable.

Only the pitch is read, not the volume: the pitch shows where the clock of
a custom instrument is, which is what the audio tests check.

특징값(첫 형식 말고 전부)은 키 24 음(기본음 261.63Hz)의 샘플 600..2600을
읽어 숫자 하나를 찍고, 오디오 스위트는 그것을 허용 오차와 함께 비교한다.

wav 파일에서 음을 다시 읽어 낸다. 소리를 WINDOW 샘플(22050Hz)짜리 창 COUNT개로
자르고, 창마다 담긴 피치를 키 24(PICO-8 트래커의 C-2) 기준 키로 찍는다. 소리가
없으면 "."이다. PICO-8의 export는 변화마다 조금 늦게(최대 한 틱, 183샘플)
시작하므로 창의 앞부분 SKIP 샘플은 버린다. 그래야 PICO-8과 pico-c의 출력을
비교할 수 있다. 음량은 읽지 않고 피치만 읽는다: 피치가 사용자 정의 악기의
시계가 어디 있는지 보여 주고, 오디오 테스트가 확인하는 것이 그것이다.
"""
import math
import struct
import sys
import wave

SKIP = 200          # samples left out at the start of each window
SILENCE = 300       # peak below this is silence

def read(path):
    w = wave.open(path)
    data = w.readframes(w.getnframes())
    return struct.unpack('<%dh' % (len(data) // 2), data)

def notes(samples, window, count):
    ups = [i + (-x) / (y - x) for i, (x, y) in enumerate(zip(samples, samples[1:]))
           if x < 0 <= y and y != x]
    out = []
    for k in range(count):
        a, b = k * window + SKIP, (k + 1) * window
        seg = samples[a:b]
        periods = sorted(22050 / (v - u) for u, v in zip(ups, ups[1:]) if a <= u and v < b)
        if not seg or max(abs(x) for x in seg) < SILENCE or not periods:
            out.append('.')
            continue
        freq = periods[len(periods) // 2]
        out.append('%d' % round(12 * math.log2(freq / 261.63)))
    return ' '.join(out)

FUNDAMENTAL = 261.63
STEADY = (600, 2600)

def component(samples, freq):
    """Amplitude of one frequency in the steady part (a single DFT bin).
    안정 구간에서 한 주파수의 진폭(DFT 한 칸)."""
    a, b = STEADY
    re = im = 0.0
    for n in range(a, b):
        w = 2 * math.pi * freq * n / 22050
        re += samples[n] * math.cos(w)
        im += samples[n] * math.sin(w)
    return 2 * math.hypot(re, im) / (b - a)

def feature(samples, name, arg):
    a, b = STEADY
    if name == 'rms':
        if arg:
            b = int(arg)
        return math.sqrt(sum(x * x for x in samples[a:b]) / (b - a)) / 16122
    if name == 'harm':
        return component(samples, arg * FUNDAMENTAL) / component(samples, FUNDAMENTAL)
    if name == 'peak':
        return component(samples, arg * FUNDAMENTAL) / component(samples, FUNDAMENTAL)
    if name == 'hold':
        runs, length = [], 1
        for x, y in zip(samples[a:b], samples[a + 1:b]):
            if x == y:
                length += 1
            else:
                runs.append(length)
                length = 1
        # PICO-8 sometimes ramps from one value to the next; the ramp's
        # 1-sample runs are not holds.  PICO-8은 가끔 값 사이를 경사로 잇는다.
        # 그 1샘플짜리 구간은 유지 구간이 아니다.
        runs = sorted(r for r in runs if r >= 3)
        return runs[len(runs) // 2] if runs else 0
    raise SystemExit('unknown feature: ' + name)

if __name__ == '__main__':
    if sys.argv[2].isdigit():
        print(notes(read(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])))
    else:
        arg = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
        print('%.3f' % feature(read(sys.argv[1]), sys.argv[2], arg))
