# pico-c — PICO-8 Compatible Emulator

A PICO-8 fantasy console emulator using z8lua (pico8 branch) for the Lua runtime, with SDL2 display/input.

## Status

- Lua VM: z8lua with full PICO-8 syntax extensions (fixed-point numbers)
- Graphics: sprites (spr/sspr), primitives (line/rect/circ/oval + fill), map, cls, camera, clip
- Font: authentic PICO-8 3×5 bitmap font (4px advance → 32×21 grid), exact `print` cursor semantics
- Boot screen: animated PICO-8 logo splash with progress sweep (skippable with any key)
- Command shell: prompt with history, tab completion, and the real PICO-8 command set;
  non-command input is evaluated as Lua (including `?expr` → `print(expr)`)
- Graphics detail: full `pal`/`palt` (draw + screen + secondary tables), `fillp`
  (primitive + sprite fill), and `tline` (mask/offset registers, transparency, sprite fill)
- Memory: peek/poke/2/4, memcpy/memset, 64KB RAM map (song area defaults to PICO-8's `0x41..0x44` "no pattern")
- Input: btn/btnp (btnp auto-repeat via `0x5f5c`/`0x5f5f`, 30/60fps aware)
- Devkit input: mouse (`stat(32)`–`stat(39)`) and keyboard (`stat(30)`/`stat(31)`)
  via `POKE(0x5f2d, flags)`; flag `0x2` maps mouse buttons to `btn(4)`/`btn(5)`,
  flag `0x4` enables pointer lock + relative mouse
- System status: `stat()` covers memory/cpu/clipboard/param/framerate, devkit
  mouse+keyboard, per-channel sfx/note, music state, and UTC/local clock
- Math/bitwise: sin, cos, atan2, sqrt, abs, flr, ceil, min, max, mid, band, bor, bxor, bnot, shl, shr, rotl, rotr
- Audio: sfx/music playback (approximate waveforms); `__sfx__`/`__music__`
  cart sections load into RAM (songs default to PICO-8's `0x41..0x44` "no pattern")
- Cartridge: `.p8` text and `.p8.png` image loading — `__lua__`, `__gfx__`
  (nibble-swapped), `__gff__`, `__map__`, `__sfx__`, `__music__`, `__label__`,
  with legacy `:c:` and `pxa` code decompression and `#include` expansion
- Lua runtime: z8lua parser fix — the lexer's string table now uses a
  string-only lookup, so carts that mix a numeric literal (e.g. `1`) with a
  numeric-looking string literal (`"1"`) no longer crash the parser

## Build

Every supported platform has its own folder under `platforms/`, with the
dependencies it needs and the scripts that build and test it.

| Platform | Build | Test |
|----------|-------|------|
| macOS | `platforms/macos/build.sh` | `platforms/macos/test.sh` |
| Linux | `platforms/linux/build.sh` | `platforms/linux/test.sh` |
| Windows (MSYS2/MinGW) | `platforms/windows/build.bat` | `platforms/windows/test.bat` |

The top level `Makefile` does the same thing and picks the platform from
`uname -s`.  Every artifact lands in `build/<platform>/`, so the platforms
never overwrite each other:

```sh
make                     # macOS, Linux or Windows
make PLATFORM=windows    # force a platform
./build/macos/pico-c tests/carts/test.p8
```

### Requirements
- C++17 compiler (g++ or clang++)
- SDL2 development libraries
- Git (to clone z8lua)

### Dependencies

| Folder | Content |
|--------|---------|
| `platforms/common/deps/z8lua` | the Lua runtime, cloned by `make` |
| `platforms/macos/deps/` | nothing vendored, SDL2 comes from MacPorts |
| `platforms/linux/deps/` | nothing vendored, SDL2 comes from the system |
| `platforms/windows/deps/SDL2-2.30.11` | the prebuilt MinGW-w64 SDL2 |

## Tests

The regression suite lives in `tests/`: `tests/carts` holds the test carts,
`tests/expected` the golden transcripts they are compared with.  The
transcripts were produced by the real PICO-8, so the suite checks pico-c
against PICO-8 on every platform.

```sh
platforms/macos/test.sh              # the whole suite
platforms/macos/test.sh cmp_all.p8   # a single cart
```

On macOS the transcripts can be compared with the real PICO-8 and refreshed:

```sh
platforms/macos/oracle.sh            # compare every cart
platforms/macos/oracle.sh --record   # refresh tests/expected/
```


## Controls

| Key   | PICO-8 Button |
|-------|--------------|
| Arrows | Directional |
| Z / C | O |
| X / V | X |
| Escape | Quit (returns to the shell) |

Carts may also use the mouse and keyboard by enabling devkit input mode with
`POKE(0x5f2d, 1)` (add `0x2` to map the mouse buttons onto `btn(4)`/`btn(5)`,
add `0x4` for pointer lock and relative movement). Mouse state is read with
`stat(32)`–`stat(39)`, keyboard with `stat(30)`/`stat(31)`.

## Console

On start the boot splash plays (~2.6s, press any key to skip), then a command
shell appears. Type a command and press Enter; Up/Down browses history and Tab
completes command or cart names.

Anything that is not a recognised command is evaluated as Lua, just like PICO-8:
type `1+2` to see `3`, `print("hi")`, or `?x` as shorthand for `print(x)`.
Syntax and runtime errors are shown in red.

| Command | Action |
|---------|--------|
| `help [topic]` | Command list, or a topic (`gfx` `data` `audio` `input` `system` `math` `lua`) |
| `run [name]` | Load (if given) and run a cart |
| `load` / `save` | Read / write a cart (`.p8` extension optional) |
| `dir` / `ls` | List cartridges in a folder |
| `cd` / `mkdir` | Change / create a directory |
| `folder [backups\|desktop]` | Open a folder in the host file browser |
| `backup` | Copy the loaded cart's source into `backup/` |
| `info` | Cart stats (chars/lines) |
| `config [key] [value]` | List / set session settings |
| `stop` / `resume` / `reset` | Stop / continue a cart; `reset` restores the draw state |
| `reboot` | Restart the boot splash |
| `cls` / `clear` | Clear the screen |
| `shutdown` / `quit` / `exit` | Exit |

Recognised PICO-8 commands that are not implemented yet (`edit`, `export`,
`import`, `splore`, `keyconfig`, `install_demos`, `install_games`, `login`,
`logout`, `upload`) print an honest "not available" message.

Passing a cart on the command line boots straight into that cart.

## License

pico-c is available under the WTFPL.
z8lua is copyright Sam Hocevar, also WTFPL.
