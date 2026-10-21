# macOS

Build and test pico-c on macOS.

## Dependencies

| What | How to get it |
|------|---------------|
| SDL2 | `sudo port install sdl2` (MacPorts, `/opt/local`) |
| A C++17 compiler | the Xcode command line tools, `xcode-select --install` |
| Git | `xcode-select --install` (needed to clone z8lua) |

Nothing is vendored for macOS, `deps/` is empty on purpose.

## Build

```sh
platforms/macos/build.sh
```

The binary lands in `build/macos/pico-c` and is code signed with an ad-hoc
signature, which macOS needs before it will run it.

## Test

```sh
platforms/macos/test.sh
```

## Compare with the real PICO-8

`/Applications/PICO-8.app` hangs on some machines, so the oracle runs from a
signed copy in `/tmp/p8app`:

```sh
platforms/macos/oracle.sh            # compare every cart
platforms/macos/oracle.sh --record   # refresh tests/expected/
platforms/macos/oracle.sh cmp_all.p8 # a single cart
```

The copy is made automatically the first time the script runs.  To use
another bundle, set `PICO8_APP`; to use another copy, set `ORACLE_DIR`.
