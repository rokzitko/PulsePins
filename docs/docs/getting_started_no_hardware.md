## Host-side work without hardware

PulsePins is primarily a hardware-backed project, but useful contribution work is possible without owning a board.

### Best entry points

* documentation in [`docs/`]({{ source_file("docs/") }}) and top-level [`README.md`]({{ source_file("README.md") }}) files
* Python bindings in [`python/`]({{ source_file("python/") }})
* C++ sequence handling and command-line behavior in [`c++/`]({{ source_file("c++/") }})
* RTL simulation and test benches in [`ip/`]({{ source_file("ip/") }})

### Useful commands

Build the documentation site:

```bash
make -C docs site
```

Build and test the Python bindings on a host machine:

```bash
make -C python USE_PREGENERATED=1 build test-host
```

This host-side path uses the checked-in pregenerated HPS header from [`c++/artifacts/`]({{ source_file("c++/artifacts/") }}) and intentionally skips tests marked as hardware-only. It is useful for syntax/import/API validation, but it is not a supported
replacement for building the production Python modules on the DE10-Nano. True Python
cross-compilation is not supported.

Run HDL test benches:

```bash
make -C ip test
```

### Good first contributions

* improve docs clarity
* add recipes and examples
* improve parser validation
* add simulation tests
* improve contributor onboarding

### Areas to treat carefully

Without hardware, avoid making strong claims about:

* timing accuracy
* active clocking behavior on the board
* exact runtime behavior of deployment scripts on the target image
* measured outputs from `ppfreq`, `ppts`, `pptemp`, and related tools

For those cases, document assumptions and leave room for later hardware verification.
