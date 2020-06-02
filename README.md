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
mkdir builddir
meson builddir
cd builddir
ninja
```
