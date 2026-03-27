## Getting started without hardware

You can contribute to PulsePins meaningfully without owning a board.

### Best entry points

* documentation in `docs/` and top-level `README*` files
* Python bindings in `python/`
* C++ sequence handling and command-line behavior in `c++/`
* RTL simulation and test benches in `ip/`

### Useful commands

Build the documentation site:

```bash
make -C docs site
```

Build and test the Python bindings:

```bash
make -C python build
make -C python test
```

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
* current clocking behavior on the board
* exact runtime behavior of deployment scripts on the target image
* measured outputs from `ppfreq`, `ppts`, `pptemp`, and related tools

For those cases, document assumptions and leave room for later hardware verification.
