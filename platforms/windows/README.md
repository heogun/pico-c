# Windows

pico-c builds on Windows in two ways, both giving a native `pico-c.exe`:
with Visual Studio (MSVC) and CMake, or with MSYS2 / MinGW-w64 and the
Makefile.

## Visual Studio (MSVC)

| What | How to get it |
|------|---------------|
| Visual Studio 2022 | with "Desktop development with C++" (MSVC, CMake) |
| SDL2 | [vcpkg](https://vcpkg.io): `vcpkg install sdl2:x64-windows`, and set `VCPKG_ROOT` |
| Git | to clone z8lua, which CMake does at configure time |
| Python 3 | for the test suite |

From a "Developer Command Prompt" (or any prompt where `cmake` works):

```bat
platforms\windows\build-msvc.bat
platforms\windows\test-msvc.bat
```

The binary lands in `build\msvc\Release\pico-c.exe`, with `SDL2.dll` next to
it.  Without vcpkg, point `SDL2_DIR` at the `cmake` folder of the SDL2
development package for Visual C++.  The repository keeps its text files
with LF line endings on every system (`.gitattributes`), which the test
carts and the z8lua patch need.

## MSYS2 / MinGW-w64

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

## Compare with PICO-8

With PICO-8 installed, the transcripts can be checked against it, like
`platforms/macos/oracle.sh` does on macOS:

```bat
platforms\windows\oracle.bat              & rem every cmp_*.p8 cart
platforms\windows\oracle.bat cmp_all.p8   & rem some carts
platforms\windows\oracle.bat --record     & rem rewrite tests\expected
```

It needs Python 3 and any pico-c build (`--bin` picks one).  PICO-8 is taken
from `C:\Program Files (x86)\PICO-8\pico8.exe` (set `PICO8` to change it) and
runs with `-home` in a temporary folder, so its own config and carts are not
touched.  Windows PICO-8 buffers its stdout when it is a pipe, so lines a
cart writes to stdout (serial output, errors) can come out after later
`printh` lines; such a cart is reported as `ORDER` instead of a difference.
