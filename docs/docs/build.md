## Build and deployment

PulsePins is built as a combined FPGA hardware, ARM software, and optional image-assembly project.

The main build entry point is the repository root [`Makefile`]({{ source_file("Makefile") }}).

### Top-level build flow

Running `make` at the repository root performs these steps:

1. Generate `base_hps.sopcinfo` from [`base_hps.qsys`]({{ source_file("base_hps.qsys") }})
2. Generate the top-level HPS header `hps_0.h` for compilation; the checked-in pregenerated copy is [`c++/artifacts/hps_0.h`]({{ source_file("c++/artifacts/hps_0.h") }})
3. Compile the [Quartus Prime](https://www.altera.com/products/development-tools/quartus) project into `pulsepins.sof`
4. Run the Quartus timing/report checker
5. Convert the SOF bitstream into `pulsepins.rbf`
6. Build the ARM C++ programs in [`c++/`]({{ source_file("c++/") }})

Relevant targets in [`Makefile`]({{ source_file("Makefile") }}):

* `all` - full hardware + C++ build
* `dev-check` - consolidated local checks
* `timing-check` - parse existing Quartus reports and enforce timing signoff checks
* `timing-sdc-check` - parse existing Quartus reports and check only project SDC handling
* `board-smoke` - fast manual live-board smoke pass against local build outputs
* `copy` - copy `pulsepins.rbf` to the target board
* `copy_boot` - copy the RBF to the boot partition path
* `copy_all` - copy hardware, C++, Python, tests, and I2C helpers to the target board
* `copy_all_img` / `copy_all_image` - stage the same content into the image tree
* `lint` - run Verible lint on top-level SystemVerilog/Verilog RTL
* `clean` - remove generated build outputs across subprojects

For a quick manual live-board regression pass after the build outputs already exist, use `make board-smoke`. That target wraps [`scripts/board_smoke.sh`]({{ source_file("scripts/board_smoke.sh") }}), redeploys the local `pulsepins.rbf` bitstream and `pptool`, `ppscpi`, and `ppwebgui` binaries, reloads the FPGA, and runs a small finite smoke sequence against the board plus the two network services, including a few selected failure-path checks (`ppscpi` error queue, `ppwebgui` HTTP `400`, and `ppwebgui` HTTP `504`). It does not rebuild those build outputs first. Override the target board with `TARGETHOST=...` when needed.

For the normal local contributor checks, use:

```bash
make dev-check
```

That target runs C++, documentation, and Python checks without running the FPGA build or accessing a live board.

### FPGA hardware build

The FPGA build depends on:

* top-level project files at the repository root
* generated Platform Designer/Qsys outputs
* IP sources under [`ip/`]({{ source_file("ip/") }})
* `*_hw.tcl` integration files used by the Quartus system description

Platform Designer/Qsys is Intel/Altera's system-integration tool for assembling FPGA IP blocks, interconnects, clocks, resets, and HPS-to-FPGA interfaces into the generated hardware system.

Important outputs:

* `base_hps.sopcinfo` - system description generated from [`base_hps.qsys`]({{ source_file("base_hps.qsys") }})
* top-level generated `hps_0.h` - HPS/FPGA address map header consumed by normal hardware/C++ builds; the checked-in pregenerated copy is [`c++/artifacts/hps_0.h`]({{ source_file("c++/artifacts/hps_0.h") }})
* `pulsepins.sof` - SRAM Object File; Quartus-generated volatile FPGA programming image used for direct FPGA programming/debug loading
* `pulsepins.rbf` - Raw Binary File; compact FPGA configuration bitstream used for boot/runtime deployment, including `FPGA-writeConfig` and boot-partition workflows

`QDIR` can be overridden to point to a local Quartus `bin` directory, and `Makefile.local` can provide local overrides without changing the tracked build file. Qsys tools are resolved from the sibling `sopc_builder/bin` directory through `QUARTUS_ROOT` and `QSYS_DIR`, so the FPGA build does not rely on global `PATH` for Quartus tools. The top-level FPGA build runs [`scripts/check_quartus_timing.py`]({{ source_file("scripts/check_quartus_timing.py") }}) after Quartus compilation by default. Use `make CHECK_QUARTUS_TIMING=0` only for local/debug builds where timing signoff should be skipped deliberately.

Clocking is a central part of the hardware build. The hardware design uses PLL-generated `core_clk` and `int_clk`, a
selectable `streamer_clk` path, and explicit top-level timing constraints in [`pulsepins.sdc`]({{ source_file("pulsepins.sdc") }}). For the detailed clocking
model and software clock control, see `clock_domain.md`.

After a Quartus build, run `make timing-check` from the repository root to re-check existing reports without rebuilding. The checker parses the Quartus reports and fails on ignored project SDC constraints, missing streamer generated clocks, PLL clock cross-check warnings, unconstrained paths, or negative timing slack. Use `make timing-sdc-check` for the narrower project-SDC-only check.

### C++ build

The ARM software lives in [`c++/`]({{ source_file("c++/") }}).

Key targets in [`c++/Makefile`]({{ source_file("c++/Makefile") }}):

* `build` - build `pptool`, `ppscpi`, `ppwebgui`, `pllcalc`, and `unit_tests`
* `copy` - copy the executables to the target board and create the usual symlinks to `pptool`
* `copy_sources` - copy source files for target-board rebuilds
* `copy_img` / `copy_sources_img` - stage executables or sources into the image tree

The build expects:

* top-level generated `hps_0.h` from the hardware build; builds on a development machine use the checked-in pregenerated [`c++/artifacts/hps_0.h`]({{ source_file("c++/artifacts/hps_0.h") }}) with `USE_PREGENERATED=1`
* SoC EDS / hwlib headers
* the Lua sources vendored under [`c++/third_party/lua`]({{ source_file("c++/third_party/lua/") }})

The DMA-backed streamer uses the FPGA design's Intel/Altera Modular Scatter-Gather DMA block through the HPS-side [`streamer_dma.hh`]({{ source_file("c++/streamer_dma.hh") }}) wrapper, so DMA support depends on both the generated hardware address map and the SoC EDS / hwlib-facing build environment.

By default the build is cross-compiling for ARM, but the sources are also structured so they can be copied to the board and built there.

`ppwebgui` and `pllcalc` are standalone executables like `ppscpi`, not `pptool` symlink modes. They are included in the normal `build` and `copy` targets.

The C++ side also participates in clock configuration. The `FPGA` wrapper and PLL helper classes can reconfigure the
internal PLLs and switch the active streamer clock source, so clocking should be thought of as a hardware/software
boundary rather than a purely RTL concern.

The C++ ownership split is deliberate:

* [`c++/startup.hh`]({{ source_file("c++/startup.hh") }}) applies the common startup behavior
* [`c++/options.hh`]({{ source_file("c++/options.hh") }}) resolves CLI/environment clocking choices into typed policy objects
* [`c++/fpga.hh`]({{ source_file("c++/fpga.hh") }}) owns top-level source switching and shared hardware state
* [`c++/pll_clk.hh`]({{ source_file("c++/pll_clk.hh") }}) owns PLL reconfiguration wrappers

### Python bindings

The Python bindings live in [`python/`]({{ source_file("python/") }}) and are built with CMake and nanobind. A board-native build needs both `pytest` and `nanobind` installed in the active Python environment.

Production Python builds are expected to happen on the DE10-Nano board itself.
Development-machine builds are useful for syntax/import/API testing, but true Python cross-
compilation is not supported.

The [`python/Makefile`]({{ source_file("python/Makefile") }}) provides:

* `build` - configure and build the extension modules under `python/build`
* `test` - run the full Python test suite, including board-backed tests
* `test-host` - run only the Python tests that do not require a board (`-m "not hardware"`)
* `copy_sources` / `copy_misc` - copy sources to the target board
* `copy_sources_img` - stage the sources into the image tree

The CMake configuration builds two modules:

* `pp`
* `pp_impl`

The `pp` module is split across multiple translation units: [`python/pp.cc`]({{ source_file("python/pp.cc") }}) contains the nanobind module entry point, while the actual bindings live in [`python/pp_bind_*.cc`]({{ source_file("python/") }}).

### IP-level simulation/testbenches

[`ip/Makefile`]({{ source_file("ip/Makefile") }}) delegates to IP subdirectories and is mainly used for RTL testbenches.

Running `make -C ip test` executes the integrated per-IP test targets for directories such as:

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
* C++ executables and source trees
* Python sources
* tests
* I2C helpers

The image workflow expects an external base image and additional binary assets that are not stored in this repository.

Use `IMAGE_ROOT=/path/to/image make copy_all_img` to stage into a different image tree. `IMGROOT` remains the home-root staging path and defaults to `$(IMAGE_ROOT)/ext/home/root`.

### Recommended workflows

Hardware + software build:

```bash
make
```

Copy the complete board bundle to the target board:

```bash
make copy_all
```

Deployment targets use `TARGETHOST` as the board host name and `SCP_TARGET` as the exact ssh/scp destination. By default `SCP_TARGET=$(TARGETHOST)`; override `SCP_TARGET=user@host` when the copy user or ssh destination differs from the board host name.

This also installs the Bash-completion file for the `pptool` command family onto the live board under `/etc/profile.d/pulsepins-completion.sh`.

Install Bash completion for the `pptool` command family on the live board:

```bash
sudo ./scripts/install_bash_completion.sh
```

The prepackaged quick-start SD-card images already ship with this completion installed, so the installer is only needed for manually provisioned systems.

If you stage a board image through the repository Makefiles, `make copy_all_img` (or `make copy_all_image`) also stages the same completion file into `image/ext/etc/profile.d/pulsepins-completion.sh`.

Build only the Python bindings:

```bash
make -C python USE_PREGENERATED=1 build
```

`USE_PREGENERATED=1` selects the checked-in pregenerated [`c++/artifacts/hps_0.h`]({{ source_file("c++/artifacts/hps_0.h") }}) instead of the top-level generated header; use it when the generated header is unavailable.

Run RTL testbenches:

```bash
make -C ip test
```
