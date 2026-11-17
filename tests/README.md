# pico-c test suite

Everything that is only used to test pico-c lives in this folder.

```
tests/
├── carts/       the test carts (.p8) and the data files they read
├── expected/    the golden transcripts the suite compares against
├── audio/       the audio suite: carts that export .wav files + expected notes
└── tools/       helper programs used by the tests
```

## carts/

| File | What it does |
|------|--------------|
| `cmp_*.p8` | the regression suite: every cart prints its result with `printh()`, so the transcript can be compared with the one of the real PICO-8 |
| `cmp_tables.p8`, `cmp_line_pen.p8`, `cmp_palette.p8`, `cmp_memwrap.p8` | behaviour found by playing real games: table helpers while the table changes, `line(x,y)`, the pen colour, the secret palette, addresses that wrap at 64KB |
| `cmp_syntax_*.p8` | one syntax error each (a block or a bracket left open, a missing goto label, a stray `end`): the error block PICO-8 prints |
| `cmp_stat_time.p8` | the clock: the values cannot be compared literally, so the cart only prints properties that hold at every point in time |
| `test.p8` | the small demo cart used to try the emulator out |
| `cstore_base.p8` | the cart `cmp_cstore.p8` writes its data area into |
| `loadA.p8`, `loadB.p8` | `load()` test: `loadA.p8` loads `loadB.p8` |
| `chess_resaved.p8` | written by `cmp_dumpresave.p8` (it re-saves the cart it loaded) |

Scratch carts (`test_*.p8`, `runtest.p8`, `p8dtest.p8`) and the files the
carts produce while they run (`cmp.p8d`, `guard.p8`, `ptest.txt`,
`runtest2.txt`, `p8save_out.p8`, `p8png_out.p8.png`) live here as well but
are not tracked by git.  `cmp_dget.p8` creates its `cmp.p8d` on its own, so
it works on a fresh checkout too.

The carts are run from this directory, so a cart that reads or writes a
sibling file finds it.

## expected/

| File | What it does |
|------|--------------|
| `<cart>.txt` | the golden transcript of a cart: what the cart prints when it runs |
| `masks.txt` | values that cannot be reproduced exactly (the clock) and are masked before the comparison |
| `divergences.txt` | the carts that differ from the real PICO-8 on purpose |

The golden transcripts were produced by the real PICO-8 with
`platforms/macos/oracle.sh --record`.  A cart without a golden transcript is
skipped by the suite.

## audio/

The sound cannot be checked through a transcript, so the audio suite
compares sound itself.  Every cart in `audio/carts/` exports its sound
effects with `export("<name>_%d.wav")`; `tools/wavnotes.py` cuts a file into
windows and prints the pitch of each window (relative to key 24, `.` when
silent), and `audio/expected/<name>.txt` holds what it prints for PICO-8's
own export of the same cart.

| File | What it does |
|------|--------------|
| `audio/carts/inst.p8` | custom instruments (waveforms 8–15): the retrigger rules, the instrument clock, effects on top, nesting, loops |
| `audio/carts/filters.p8` | the SFX filters: buzz on every waveform, dampen, detune, reverb, noiz, filters of an instrument |
| `audio/carts/noise.p8` | the loudness of the plain noise at four keys (random, so compared within a tolerance) |
| `audio/carts/timing.p8` | effects worked out once per tick: the arpeggio steps, the silent first tick of a fade in |
| `audio/expected/<name>.txt` | one check per line, recorded with `platforms/macos/oracle_audio.sh --record` (see below) |

A line is either `<sfx> <window> <count> | <notes>` (the notes, compared
exactly) or `<sfx> <feature> [arg] | <value> ~<tol>` (a number compared
within a tolerance): `rms` (loudness), `harm K` (the K-th harmonic against
the fundamental), `peak R` (the spectrum at R times the fundamental: a
second voice), `hold` (the hold length of noiz noise).

Only the pitch is compared, and the first 200 samples of every window are
skipped: PICO-8's export starts every change a little late (by up to one
tick), and its start-up delay changes from run to run.  Cases that depend
on that timing (slides and fades read sample by sample) are left out.

PICO-8 only renders sound in a windowed run, so `oracle_audio.sh` opens a
window for a few seconds.  It gives PICO-8 a home folder of its own
(`-home`), because a personal config with `sound_volume 0` exports silence.

## tools/

`wavnotes.py` reads the notes back out of a .wav file (see `audio/`).

`dumpcart.c` prints what a `.p8.png` cartridge contains.  Build it with:

```sh
cc -o build/macos/dumpcart tests/tools/dumpcart.c
```

## Running the suite

The suite runs on every platform, it does not need PICO-8:

```sh
platforms/macos/test.sh      # macOS
platforms/linux/test.sh      # Linux
platforms/windows/test.bat   # Windows (MSYS2)
```

On macOS the transcripts can also be checked against the real PICO-8:

```sh
platforms/macos/oracle.sh            # compare every cart
platforms/macos/oracle.sh --record   # refresh tests/expected/
platforms/macos/oracle_audio.sh      # the audio suite against PICO-8
```
