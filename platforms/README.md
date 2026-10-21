# platforms

Every supported platform has its own folder.  A folder holds the
dependencies of that platform and the scripts that build and test pico-c on
it.

```
platforms/
├── common/    dependencies and helpers shared by every platform
├── macos/     macOS   — SDL2 from MacPorts (/opt/local)
├── linux/     Linux   — SDL2 from the system
└── windows/   Windows — MSYS2/MinGW-w64 and a vendored SDL2
```

## The scripts

| Script | What it does |
|--------|--------------|
| `build.sh` / `build.bat` | build pico-c, the binary lands in `build/<platform>/` |
| `test.sh` / `test.bat` | run the regression suite of `tests/` |
| `oracle.sh` (macOS only) | compare pico-c with the real PICO-8 and refresh the golden transcripts |

The `.bat` files are the Windows entry points; they need the MSYS2 tools
(`make`, `bash`).  The `.sh` files work on every POSIX shell.  On macOS and
Linux the `.bat` files only print a hint, because those platforms are built
with their shell script.

## The dependencies

| Folder | Content |
|--------|---------|
| `common/deps/z8lua` | the Lua runtime ([z8lua](https://github.com/samhocevar/z8lua), `pico8` branch).  It is cloned by `make` and shared by every platform. |
| `macos/deps/` | nothing is vendored: SDL2 comes from MacPorts |
| `linux/deps/` | nothing is vendored: SDL2 comes from the distribution |
| `windows/deps/SDL2-2.30.11` | the prebuilt MinGW-w64 SDL2 |

The dependencies are not tracked by git, see `.gitignore`.
