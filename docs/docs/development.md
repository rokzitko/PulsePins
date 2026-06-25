## PulsePins development

This page documents some of the development practices and standards followed in the project.

### Coding standards

Use structured, clear code. Prefer uniform and self-describing variable names. Add inline documentation where it helps.
Comments should explain intent. Avoid hard-coded parameters. Keep tests broad enough to cover corner cases.

### TO DO list symbols

The following symbols are used in project TODO lists:

- ``!`` important, high-impact item
- ``@`` complex task, might be time consuming or technically demanding
- ``o`` recurring tasks, to be performed periodically (keep them on the list)
- ``#`` major milestones, enabler for other tasks
- ``-`` easy task
- ``x`` low-impact, low-priority

### Source code layout

* [`ip/`]({{ source_file("ip/") }}) - Verilog descriptions of circuitry
* [`ip/combiner/`]({{ source_file("ip/combiner/") }}) - IP for the advanced multiplexer
* [`ip/combiner_comb/`]({{ source_file("ip/combiner_comb/") }}) - combinational version of the advanced multiplexer
* [`ip/combiner_trig/`]({{ source_file("ip/combiner_trig/") }}) - trigger-signal multiplexer
* [`ip/counter/`]({{ source_file("ip/counter/") }}) - event counters and test/measurement logic
* [`ip/freq_meter/`]({{ source_file("ip/freq_meter/") }}) - frequency-meter core
* [`ip/misc/`]({{ source_file("ip/misc/") }}) - small reusable support blocks
* [`ip/st_mux/`]({{ source_file("ip/st_mux/") }}) - Avalon-ST multiplexer, implemented in [`st_mux_if.sv`]({{ source_file("ip/st_mux/st_mux_if.sv") }})
* [`ip/streamer/`]({{ source_file("ip/streamer/") }}) - IP for the run-length decoder engine
* [`ip/rl_encoder_if/`]({{ source_file("ip/rl_encoder_if/") }}) - IP for the run-length encoder engine
* [`ip/ts_core/`]({{ source_file("ip/ts_core/") }}) - IP for timestamp capture / time tagging
* [`c++/`]({{ source_file("c++/") }}) - C++ source code for API, pptool
* [`python/`]({{ source_file("python/") }}) - Python binding


### Documentation

These manual pages are built with [MkDocs](https://www.mkdocs.org/), using macros plugin. For testing the generated web site, we use
[caddy](https://caddyserver.com/).

Useful starting points for the codebase:

* `build.md` - hardware/software build and deployment flow
* `clock_domain.md` - detailed clocking, CDC, and timing-constraint reference
* `cpp.md` - C++ API overview
* `combiner.md` - output and trigger combiner architecture
* `counter.md` - integrated measurement/counter subsystem
* `timestamp.md` - timestamp capture path
* `freq_meter.md` - frequency-meter block and API
* `st_mux.md` - Avalon-ST multiplexer helper
* `misc_ip.md` - reusable support RTL blocks
