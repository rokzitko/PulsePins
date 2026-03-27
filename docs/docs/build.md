## Build and deployment

PulsePins is built as a combined FPGA hardware, ARM-side software, and optional image-assembly project.

The main entry point is the repository root `Makefile`.

### Top-level build flow

Running `make` at the repository root performs these steps:

1. Generate `base_hps.sopcinfo` from `base_hps.qsys`
2. Generate the HPS header `hps_0.h`
3. Compile the Quartus project into `pulsepins.sof`
4. Convert the SOF bitstream into `pulsepins.rbf`
5. Build the ARM-side C++ programs in `c++/`

Relevant targets in `Makefile`:

* `all` - full hardware + C++ build
* `copy` - copy `pulsepins.rbf` to the target host
* `copy_boot` - copy the RBF to the boot partition path
* `copy_all` - copy hardware, C++, Python, tests, and I2C helpers to the target host
* `copy_all_img` - stage the same content into the image tree
* `lint` - run Verible lint on top-level Verilog/SystemVerilog
* `clean` - remove generated artifacts across subprojects

### FPGA hardware build

The FPGA build depends on:

* top-level project files at the repository root
* generated Platform Designer/Qsys outputs
* IP sources under `ip/`
* `*_hw.tcl` integration files used by the Quartus system description

Important outputs:

* `base_hps.sopcinfo` - system description generated from `base_hps.qsys`
* `hps_0.h` - HPS/FPGA address map header consumed by the C++ build
* `pulsepins.sof` - SRAM programming image
* `pulsepins.rbf` - raw binary file used for boot/runtime deployment

`QDIR` can be overridden to point to a local Quartus installation, and `Makefile.local` can provide local overrides without changing the tracked build file.

### C++ build

The ARM-side software lives in `c++/`.

Key targets in `c++/Makefile`:

* `build` - build `pptool`, `ppscpi`, and `unit_tests`
* `copy` - copy the executables to the target host and create the usual symlinks to `pptool`
* `copy_sources` - copy source files for on-target rebuilds
* `copy_img` / `copy_sources_img` - stage executables or sources into the image tree

The build expects:

* `hps_0.h` from the top-level hardware build
* SoC EDS / hwlib headers
* the Lua sources vendored under `c++/third_party/lua`

By default the build is cross-compiling for ARM, but the sources are also structured so they can be copied to the board and built there.

### Python bindings

The Python bindings live in `python/` and are built with CMake and nanobind.

The `python/Makefile` provides:

* `build` - configure and build the extension modules under `python/build`
* `test` - run `pytest test.py`
* `copy_sources` / `copy_misc` - copy sources to the target host
* `copy_sources_img` - stage the sources into the image tree

The CMake configuration builds two modules:

* `pp`
* `pp_impl`

### IP-level simulation/test benches

`ip/Makefile` delegates to IP subdirectories and is mainly used for HDL-level test benches.

Running `make -C ip test` executes the per-IP test targets for directories such as:

* `combiner`
* `combiner_comb`
* `combiner_trig`
* `counter`
* `rl_encoder_if`
* `st_mux`
* `streamer`
* `ts_core`

### Image assembly

The `image/` directory stages files into an SD-card image tree for the DE10-Nano.

The top-level `copy_all_img` target populates `image/ext/home/root/` with:

* the FPGA RBF image
* ARM-side binaries and source trees
* Python sources
* tests
* I2C helpers

The image workflow expects an external base image and additional binary assets that are not stored in this repository.

### Recommended workflows

Hardware + software build:

```bash
make
```

Copy the current runtime artifacts to the board:

```bash
make copy_all
```

Build only the Python bindings:

```bash
make -C python build
```

Run HDL test benches:

```bash
make -C ip test
```
