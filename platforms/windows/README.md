# Windows

Build and test pico-c on Windows with MSYS2 / MinGW-w64.

## Dependencies

| What | How to get it |
|------|---------------|
| MSYS2 | <https://www.msys2.org/>, then `pacman -S mingw-w64-x86_64-gcc make` |
| SDL2 for MinGW | the prebuilt `SDL2-2.30.11` in `deps/SDL2-2.30.11` |
| Git | comes with MSYS2 (needed to clone z8lua) |

`deps/SDL2-2.30.11/x86_64-w64-mingw32` holds the headers (`include`) and the
import libraries (`lib`) of a MinGW-w64 build of SDL2.  The folder is not
tracked by git; replace it with your own build of SDL2 or point `SDL2_DIR`
somewhere else.

## Build

From an MSYS2 shell:

```sh
platforms/windows/build.sh      # or build.bat from a plain cmd.exe
```

The binary lands in `build/windows/pico-c.exe`.

## Test

```sh
platforms/windows/test.sh       # or test.bat from a plain cmd.exe
```

The batch files need the MSYS2 tools (`make`, `bash`) on the `PATH`; they
look for them in `C:\msys64\usr\bin` and add `C:\msys64\mingw64\bin` (where
the MinGW compiler lives) to the `PATH` as well.
