# Linux

Build and test pico-c on Linux.

## Dependencies

| What | How to get it |
|------|---------------|
| SDL2 | `sudo apt install libsdl2-dev` (Debian, Ubuntu) or `sudo dnf install SDL2-devel` (Fedora) |
| A C++17 compiler | `sudo apt install g++` |
| Git | `sudo apt install git` (needed to clone z8lua) |

Nothing is vendored for Linux, `deps/` is empty on purpose.

## Build

```sh
platforms/linux/build.sh
```

The binary lands in `build/linux/pico-c`.

## Test

```sh
platforms/linux/test.sh
```

The suite runs the carts headless, so it does not need a display.  When SDL2
cannot open a window the emulator still runs the cart.
