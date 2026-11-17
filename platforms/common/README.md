# common

The dependencies and the helpers every platform shares.

## deps/z8lua

The Lua runtime: [z8lua](https://github.com/samhocevar/z8lua), `pico8`
branch.  `make` clones it into this folder the first time it is needed, so
the folder is empty on a fresh checkout and is not tracked by git.

The checkout is pinned to revision `8575dfbc` (`Z8LUA_REV` in the Makefile)
and `make` applies `z8lua.patch` to it:

| File | Change |
|------|--------|
| `llex.c` | the lexer's string table uses a string-only lookup, so `1` and `"1"` in one chunk no longer crash the parser |
| `lparser.c` | nested short `if` / `while` on one line, `if cond do` |
| `ltable.c` | a string key that looks like a number is that number (`t["1"]` is `t[1]`), like PICO-8; keys that cannot be numbers skip the conversion |
| `lpico8lib.c` | `sqrt` computes the same bits as PICO-8's abacus loop with the FPU |

To change the runtime, edit the files in `deps/z8lua` and refresh the patch
with `git -C platforms/common/deps/z8lua diff > platforms/common/z8lua.patch`.

## The helper scripts

| Script | What it does |
|--------|--------------|
| `common.sh` | the paths of the repository and the normalisation of a transcript |
| `build_common.sh` | checks the toolchain of a platform and calls `make` |
| `test_common.sh` | runs the carts of `tests/carts` and compares them with `tests/expected` |
