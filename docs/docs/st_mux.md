# Avalon-ST multiplexer

The `st_mux` block is a small [Avalon-ST](https://www.intel.com/content/www/us/en/docs/programmable/683091/22-3/avalon-streaming-interfaces.html) routing helper used to select between two streaming sources.

The hardware implementation is [`ip/st_mux/st_mux_if.sv`]({{ source_file("ip/st_mux/st_mux_if.sv") }}), and the C++ control wrapper is [`c++/st_mux.hh`]({{ source_file("c++/st_mux.hh") }}).

For a maintainer-oriented RTL map, see [`ip/st_mux/README.md`]({{ source_file("ip/st_mux/README.md") }}).

### Hardware behavior

The block accepts two Avalon-ST inputs and forwards one of them to a single Avalon-ST output.

It also maintains 64-bit counters for traffic observed on each input path.

Only the selected input sees `ready` asserted, so the block acts as a strict selector rather than as a merge or arbitration stage.

For selected-path latency notes, see [RTL latency and timing](latency.md).

The control interface is based on [Avalon-MM](https://www.intel.com/content/www/us/en/docs/programmable/683091/22-3/avalon-memory-mapped-interfaces.html) and allows software to:

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

It is useful when two producers share one downstream Avalon-ST consumer and software needs to decide which path is active while keeping simple statistics for both sources.

### Related pages

* [C++ application programming interface](cpp.md)
* [Build and deployment](build.md)
