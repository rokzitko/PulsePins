## Avalon-ST multiplexer

The `st_mux` block is a small Avalon-ST routing helper used to select between two streaming sources.

The hardware implementation is `ip/st_mux/st_mux_if.sv`, and the C++ control wrapper is `c++/st_mux.hh`.

### Hardware behavior

The block accepts two Avalon-ST inputs and forwards one of them to a single Avalon-ST output.

It also maintains 64-bit counters for traffic observed on each input path.

The control interface is Avalon-MM based and allows software to:

* choose the active source channel
* read low/high words of the two traffic counters

### C++ interface

The `st_mux` class provides:

* `channel(ch)` - select input channel `1` or `2`
* `ctr1()` - read the input-1 transfer counter
* `ctr2()` - read the input-2 transfer counter
* `report()` - print both counters in human-readable form

### When it matters

This block is primarily an integration component.

It is useful when two producers share one downstream Avalon-ST consumer and software needs to decide which path is active while still keeping simple statistics for both sources.

### Related pages

* `cpp.md`
* `build.md`
