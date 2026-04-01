# pptool

``pptool`` is a single executable for performing a range of tasks. Several symbolic links point to the same
executable. The exact functionality depends on the executable called.

The network server is provided by the separate ``ppscpi`` executable, not by a ``pptool`` symlink.

## Dispatch model

The main executable entry point lives in `c++/pptool.cc`.

At startup it performs the shared host-side bootstrap:

1. parse command-line options
2. enable the common runtime policy from `startup.hh`
3. construct the single `FPGA` object
4. apply the default clock/PLL startup policy
5. dispatch to a tool-specific handler based on the executable name

This means `pptool`, `ppfg`, `ppcounter`, `ppdelay`, and several other commands are all different front doors into the same host-side runtime.

The command catalog itself is declared in `c++/pptool_commands.hh` and implemented mainly in:

* `c++/pptool_streaming.cc`
* `c++/pptool_measurement.cc`
* `c++/ppscpi.cc`

## Command families

Broadly, the commands fall into these groups:

* streaming/output generation - `ppfg`, `ppdelay`, `ppvcd`, `pptest`, `ppdmatest`, `ppmstest`
* trigger/output debugging - `pptrig`, `ppqout`, `ppaux`, `ppreset`
* measurement/readback - `ppread`, `ppcounter`, `ppts`, `ppfreq`, `pptemp`, `ppgpsdo`
* onboarding/smoke test - `pphelloworld`
* remote-control server - `ppscpi`

Many streaming-oriented commands share the same lower-level execution path in `ppworkflow.hh`, which handles sequence transmission, trigger control, optional readback verification, completion checks, FIFO accounting, and CRC comparison.

## How to extend it

To add a new command mode:

1. declare a new `pp...` handler in `c++/pptool_commands.hh`
2. implement the handler in the appropriate `pptool_*.cc` file
3. register it in the dispatch table in `c++/pptool.cc`
4. add user-facing docs under `docs/docs/`

This keeps the high-level user interface stable while letting the internal implementation evolve.

## Related docs

* [pptest](pptest.md)
* [ppmstest](pptest.md#ppmstest)
* [ppdmatest](pptest.md#ppdmatest)
* [ppfg](ppfg.md)
* [ppdelay](ppdelay.md)
* [ppreset](ppreset.md)
* [pptrig](pptrig.md)
* [ppqout](ppqout.md)
* [ppaux](ppaux.md)
* [ppcounter](ppcounter.md)
* [ppts](ppts.md)
* [ppgpsdo](ppgpsdo.md)
* [pptemp](pptemp.md)
* [ppfreq](ppfreq.md)
* [ppvcd](ppvcd.md)
* [pphelloworld](pphelloworld.md)

Related non-symlink tool:

* [ppscpi](ppscpi.md)

See also:

* [C++ API](cpp.md)
* `c++/README.md`
