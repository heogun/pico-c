# common

The dependencies and the helpers every platform shares.

## deps/z8lua

The Lua runtime: [z8lua](https://github.com/samhocevar/z8lua), `pico8`
branch.  `make` clones it into this folder the first time it is needed, so
the folder is empty on a fresh checkout and is not tracked by git.

## The helper scripts

| Script | What it does |
|--------|--------------|
| `common.sh` | the paths of the repository and the normalisation of a transcript |
| `build_common.sh` | checks the toolchain of a platform and calls `make` |
| `test_common.sh` | runs the carts of `tests/carts` and compares them with `tests/expected` |
