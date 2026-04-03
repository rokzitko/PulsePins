# Python bindings

PulsePins uses [nanobind](https://nanobind.readthedocs.io/en/latest/) to provide Python bindings for the underlying C++ interface.

The Python binding tree lives in `python/` and builds two modules:

* `pp`
* `pp_impl`

## Supported build modes

There are two practical ways to build/test the Python bindings today:

* build on the DE10-Nano board - this is the supported production path
* build on a host machine - useful for syntax/import/API testing only

True cross-compilation of the Python bindings is not currently supported.

## Board build

On the DE10-Nano, the normal workflow is:

```bash
cd python
pip3 install pytest
make
make test
```

The default build now uses `-O2` and omits `-g` to reduce memory pressure on the board.
If you need debug symbols while developing the bindings, use:

```bash
make PY_DEBUG=1
```

## Host-side testing

Host-side builds are still useful for checking that the binding code compiles and imports cleanly.
That is helpful for contributor workflows without a board, but it should not be treated as a
replacement for the board build.

## Testing expectations

`make -C python test` currently runs `pytest python/test.py`.

Some Python tests exercise board-backed MMIO/FPGA behavior, so they should not be treated as a
strictly hardware-free test battery.

See also:

* `python/README`
* `python/README.devel`
* `docs/docs/build.md`
