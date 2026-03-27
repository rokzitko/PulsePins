Tools for building the SD card image for the DE10-Nano board.

First build PulsePins by running `make` in the repository root. Then run the scripts in this directory to build the image.

The binary files to be placed in `$HOME/yocto` are distributed separately; they are not part of the GitHub repository.

`local` -> `/usr/local` (contains `jed`, `mc`, `ninja`, `meson`, and supporting libraries such as `glib` and `slang`)
`src` -> `/home/root/src` (corresponding source files)
`python` -> `/home/root/python/build` (precompiled Python bindings)
`site-packages` -> `/usr/lib/python3.8/site-packages` (preinstalled `pytest`, `nanobind`, and supporting packages)
`rsYocto_1_042_D10NANO.img` -> original rsyocto image
