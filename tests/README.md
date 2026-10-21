# pico-c test suite

Everything that is only used to test pico-c lives in this folder.

```
tests/
├── carts/       the test carts (.p8) and the data files they read
├── expected/    the golden transcripts the suite compares against
└── tools/       helper programs used by the tests
```

## carts/

| File | What it does |
|------|--------------|
| `cmp_*.p8` | the regression suite: every cart prints its result with `printh()`, so the transcript can be compared with the one of the real PICO-8 |
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

## tools/

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
```
