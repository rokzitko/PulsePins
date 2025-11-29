Tools for building the SD card image for the DE10 board.

First build PulsePins (run 'make' in base directory). Then run scripts in this directory to build the image.

The binary files to be placed in $HOME/yocto are distributed separately (i.e., they are not part of the github repository).

local -> /usr/local (contains jed, mc, ninja, meson builds and supporting libraries, glib and slang)
src -> /home/root/src (corresponding source files)
python -> /home/root/python/build (precompiled python bindings)
site-packages -> /usr/lib/python3.8/site-packages (preinstalled pytest and nanobind + supporting packages)
rsYocto_1_042_D10NANO.img -> original rsyocto image
