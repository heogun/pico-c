# Linux dependencies

Nothing is vendored for Linux.

SDL2 comes from the distribution:

```sh
sudo apt install libsdl2-dev      # Debian, Ubuntu
sudo dnf install SDL2-devel       # Fedora
```

The build uses `/usr` by default.  Another prefix can be used with
`SDL2_DIR`:

```sh
SDL2_DIR=/usr/local platforms/linux/build.sh
```
