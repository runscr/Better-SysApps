# Better SysApps
> A fork of [Better Settings](https://github.com/Fangal-Airbag/Better-Settings) by Fangal-Airbag

## Overview
Better SysApps is an aroma plugin that improves the Wii U System Applications by doing the following:
- Adds support for viewing Gamepad only information on the TV
- Adds input support for Wii U Pro Controllers and Wii Classic Controllers on Gampad only system titles
- Does the above ***without*** ever needing a real Gamepad!
- Can assist in setting up a Wii U (after factory reset) without a Gamepad!

## Building
To build this make sure you have the following installed:
- [wut](https://github.com/devkitPro/wut)
- [wups](https://github.com/wiiu-env/WiiUPluginSystem)
- [libmappedmemory](https://github.com/wiiu-env/libmappedmemory)

Then just run `make` in the same directory as the Makefile and place the resulting `.wps` in `sd:/wiiu/envioronments/aroma/plugins`.
<br>
<br>

## Building using the Dockerfile
It's possible to use a docker image for building. This way you don't need anything installed on your host system.

```
# Build docker image (only needed once)
docker build . -t bettersysapps-builder

# make
docker run -it --rm -v ${PWD}:/project bettersysapps-builder make

# make clean
docker run -it --rm -v ${PWD}:/project bettersysapps-builder make clean
```

# Special thanks!
Special thanks to the following, this plugin would not be possible without them!
- [Fangal-Airbag](https://github.com/Fangal-Airbag)
- Lynx64
- [Maschell](https://github.com/Maschell)
- The Cemu Project
