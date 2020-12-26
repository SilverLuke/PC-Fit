# PC-Fit
A GTK interface for using the Nintendo Balance Board 

## Dependencies

This software require these build dependencies:

```
Meson
xwiimote
toml.c
GTK3
```

## Build

```
git submodule update --init
meson builddir
cd builddir
ninja
```

## How to use meson with clion

https://www.jetbrains.com/help/clion/compilation-database.html
https://www.jetbrains.com/help/clion/custom-build-targets.html
