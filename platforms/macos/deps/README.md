# macOS dependencies

Nothing is vendored for macOS.

SDL2 is installed system wide with MacPorts:

```sh
sudo port install sdl2
```

The build uses `/opt/local` by default.  Another prefix can be used with
`SDL2_DIR`:

```sh
SDL2_DIR=/opt/homebrew platforms/macos/build.sh
```
