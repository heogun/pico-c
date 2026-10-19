# PICO-8 API Compatibility Checklist (target: User Manual v0.2.7)

Legend: `[x]` implemented, `[~]` partial/stub, `[ ]` missing.

## System
- [x] `_update()` / `_draw()` / `_init()` callback support
- [x] `_update60()` 60fps mode (input/state advance once per logical update frame)
- [~] `load(filename, breadcrumb, param_str)` (loads + runs the cart, sets
      `stat(6)` param and `stat(100)` breadcrumb, returns a boolean; resolves
      relative to the cwd then the current cart's directory)
- [x] `save(filename)` (writes a `.p8` from live RAM + the preserved `__lua__`
      source; appends `.p8` when no extension is given)
- [x] `folder` (opens the current directory in the host file browser)
- [x] `ls(directory)` (basic filtered listing)
- [x] `run(param_str)` (restarts the current cart with a new param string)
- [x] `stop(message)`
- [x] `assert(condition, message)` (raises a runtime error that stops the cart)
- [x] `reboot`
- [x] `reset()` (restores RAM 0x5f00..0x5f7f, per the manual)
- [x] `info()` (prints chars/lines to the console and returns nil, like PICO-8;
      headless `-x` does not capture that console output, so the token count is
      verified through the 8192-token load limit instead — see progress.md §3-4)
- [x] `__lua__` load limits: over 65536 bytes → `could not load <path>`; over
      8192 tokens → `program too large` + `N / 8192 tokens` and the cart is
      loaded but never run (`src/tokens.cpp`, `check_source_limits()` in
      `src/cart.cpp`)
- [x] `flip()`
- [x] `printh(str, filename, overwrite, save_to_desktop)` (default appends,
      `overwrite` truncates, `"@clip"` copies to the host clipboard, and
      `save_to_desktop` writes into `~/Desktop`)
- [x] `time()` / `t()`
- [x] `stat(x)` — `0` (memory), `1` (cpu), `4` (clipboard), `6` (param str),
      `7` (framerate), `30`/`31` (devkit keyboard), `32`–`39` (devkit mouse),
      `46`–`49` (channel sfx), `50`–`53` (channel note), `54`–`57` (music),
      `80`–`85` (UTC), `90`–`95` (local), `100` (breadcrumb label)
- [~] `extcmd(cmd_str, p1, p2)` (`reset`, `shutdown`, `pause` (falls back to the
      shell), `folder` and `go_back` are handled; `label`/`screen`/`video`/
      `rec`/`audio_*`/`set_filename` are host editor features with no equivalent)
- [x] `menuitem(index, [label], [callback])` (1..5 with button mask in bits
      0xff00; items are stored and can be listed/invoked from the shell's
      `menu` command, since pico-c has no modal pause menu)
- [x] `serial(channel, address, length)` (accepted and ignored — there is no
      GPIO device on desktop, matching PICO-8 without a GPIO server)

## Graphics
- [x] `clip(x, y, w, h, clip_previous)`
- [x] `pset`, `pget`, `sget`, `sset`
- [x] `fget`, `fset`
- [x] `print`, `cursor`, `color`, `cls`, `camera`
- [x] `circ`, `circfill`, `oval`, `ovalfill`
- [x] `line`, `rect`, `rectfill`
- [x] `rrect`, `rrectfill`
- [x] `pal(c0, c1, p)` / `pal(tbl, p)` (draw + screen + secondary tables, reset forms)
- [x] `palt(c, t)`
- [x] `spr`, `sspr`
- [x] `fillp(p)` (primitive + sprite fill, transparency/apply-to-sprites flags)

## Table Functions
- [x] `add`, `del`, `deli`, `count`
- [x] `all`, `foreach`, `pairs`

## Input
- [x] `btn(b, pl)` (reads the RAM mirror at 0x5f4c, so POKE works)
- [x] `btnp(b, pl)` (auto-repeat via 0x5f5c/0x5f5f, 30/60fps aware)
- [x] Devkit mouse input via `POKE(0x5f2d, flags)` (`stat(32)`–`stat(39)`),
      flag `0x2` maps mouse buttons onto `btn(4)`/`btn(5)`, flag `0x4` enables
      pointer lock / `stat(38)`/`stat(39)` relative movement
- [x] Devkit keyboard input via `POKE(0x5f2d, 1)` (`stat(30)` boolean,
      `stat(31)` character string); text + Enter/Backspace/Tab/Delete captured

## Audio
- [x] `sfx(n, channel, offset, length)` (n `-1` stops a channel / all non-music
      channels, n `-2` releases looping, channel `-1` auto-selects, channel `-2`
      stops that sfx on any channel; per-channel state matches PICO-8)
- [x] `music(n, fade_len, channel_mask)` (pattern sequencing with start/loop/stop
      flags, ms fade, reserved-channel mask, `stat(54)`–`stat(57)`)
- [x] `__sfx__` / `__music__` cart sections load into RAM (68 B/sfx, 4 B/song)
- [x] Song RAM defaults to PICO-8's `0x41..0x44` "no pattern" per song
- [x] SFX note decoding (pitch/waveform/volume/effect), C-0..A440 tuning,
      speed = 183/22050 s per unit, 8 waveforms, slide/vibrato/drop/fade/arp
      effects and loop start/end
- [~] Custom instruments (waveforms 8–15) and POKE-able filters/detune are not
      synthesized (built-in waveforms 0–7 are)

## Map
- [x] `mget`, `mset`
- [x] `map(...)` (skip colour 0 unless `POKE(0x5f36,0x8)`; LAYERS draws
      sprites with any matching flag bit)
- [x] `tline(...)` (mask/offset registers 0x5f38–0x5f3b, pixel mode, LAYERS
      any-flag match, background fill, sprite fill)

## Memory
- [x] `peek`, `poke`, `peek2`, `poke2`, `peek4`, `poke4`
- [x] `memcpy`, `memset`
- [x] `reload(dest, source, len, filename)` (copies the cart's on-disk data area
      0x0000..0x42ff into RAM; an optional `filename` reads another cart)
- [x] `cstore(dest, source, len, filename)` (copies RAM into the cart's data area
      and persists it back to the backing `.p8` file)
- [x] proper cart storage for reload/cstore

## Math
- [x] `max`, `min`, `mid`, `flr`, `ceil`
- [x] `cos`, `sin`, `atan2`, `sqrt`, `abs`
- [x] `rnd`, `srand`
- [x] `sgn`, bitwise ops, shifts/rotations

## Custom Menu Items
- [~] `menuitem(index, label, callback)`

## Strings and Type Conversion
- [x] `tostr`, `tonum`, `chr`, `ord`, `sub`, `split`, `type`

## Cartridge Loading
- [x] `.p8` text carts: `__lua__`, `__gfx__` (nibble-swapped: second digit is the
      high nibble), `__gff__`, `__map__`, `__sfx__` (4 header bytes + 32 notes),
      `__music__` (flags folded into the top bit of each channel byte), `__label__`
- [x] `.p8.png` image carts (sprite/RAM banks + compressed code area)
- [x] legacy `:c:` and `pxa` code decompression
- [x] `#include` expansion relative to the cart's directory
- [x] `.p8` writer: serializes live RAM (`__gfx__` nibble-swapped, `__gff__`,
      `__map__`, `__sfx__`, `__music__`) plus the preserved `__lua__` source,
      trimming trailing default lines and omitting empty sections
- [x] Lua `load(filename, breadcrumb, param_str)` returns a boolean; `save(filename)`
      writes a `.p8` (non-interactive `-x` PICO-8 defers the actual cart switch)

## Cartridge Data
- [x] `cartdata(id)` (file-backed)
- [x] `dget`, `dset` (file-backed)

## GPIO
- [~] `serial(channel, address, length)`

## Mouse and Keyboard Input
- [x] Devkit input mode `POKE(0x5f2d, flags)` (0x1 enable, 0x2 mouse→btn, 0x4 pointer lock)
- [x] Mouse: `stat(32)` x, `stat(33)` y, `stat(34)` buttons, `stat(36)` wheel,
      `stat(38)`/`stat(39)` relative movement
- [x] Keyboard: `stat(30)` keypress available, `stat(31)` character string

## Macro/Meta
- [x] token parity — the 8192-token limit, the 65536-byte source limit and both
      messages match the oracle byte for byte (progress.md §3-4)
- [ ] performance parity (fixed-point maths speed, profiling)
- [x] hidden/global API parity — `pairs(_ENV)` now enumerates exactly the same
      176 names as the oracle (`_pausemenu`, `__type`, `cocreate`/`coresume`/
      `costatus`/`yield`, `inext`, `trace`/`mapdraw`/`_set_fps`/… stubs and the
      26 glyph constants); stock Lua globals that PICO-8 lacks (`_G`, `_VERSION`,
      `pcall`, `xpcall`, `error`, `tonumber`, `dofile`, `loadfile`,
      `collectgarbage`, `string`, `table`, `coroutine`, `debug`) are removed.
      Known remaining gap: PICO-8 stores glyph characters as single charset
      bytes (codes 128..153) while pico-c keeps raw UTF-8, so `#"⬅️"` is 1 on
      PICO-8 and 6 here, and string keys sort differently. See progress.md §5.

## Console (shell)
- [x] command dispatch + history + tab completion
- [x] `help [topic]` (gfx/data/audio/input/system/math/lua + `system commands`)
- [x] `load`, `save`, `run`, `stop`, `resume`, `reset`, `reboot`, `shutdown`, `cls`
- [x] `dir`/`ls`, `cd`, `mkdir`, `folder [backups|desktop|config|bbs]`, `backup`, `info`, `config`
- [x] non-command input is evaluated as Lua (`?expr` = `print(expr)`), incl. statement-first semantics
- [ ] `edit` (built-in editor), `splore`, `keyconfig`, `install_demos`/`install_games`, `import`/`export`, `login`/`logout`/`upload` — recognised, print honest "not available" messages
