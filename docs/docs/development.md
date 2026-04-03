## PulsePins development

Users are encouraged to participate in the further development of PulsePins by contributing code or by making reasonable
feature requests. This page documents some of the development practices and standards followed in the project.

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

    ip/              # Verilog descriptions of circuitry
      combiner/      # IP for the advanced multiplexer
      combiner_comb/ # Combinational version of the advanced multiplexer
      combiner_trig/ # Trigger-signal multiplexer
      counter/       # Event counters and test/measurement logic
      freq_meter/    # Frequency-meter core
      misc/          # Small reusable support blocks
      st_mux/        # Avalon-ST multiplexer (implemented in st_mux_if.sv)
      streamer/      # IP for the run-length decoder engine
      rl_encoder_if/ # IP for the run-length encoder engine
      ts_core/       # IP for timestamp capture / time tagging
    c++/             # C++ source code for API, pptool
    python/          # Python binding


### Documentation

These manual pages are built with [MkDocs](https://www.mkdocs.org/). For testing the generated web site, we use
[caddy](https://caddyserver.com/).

Useful starting points for the current codebase:

* `build.md` - hardware/software build and deployment flow
* `clock_domain.md` - detailed clocking, CDC, and timing-constraint reference
* `cpp.md` - C++ API overview
* `combiner.md` - output and trigger combiner architecture
* `counter.md` - integrated measurement/counter subsystem
* `timestamp.md` - timestamp capture path
* `freq_meter.md` - frequency-meter block and API
* `st_mux.md` - Avalon-ST multiplexer helper
* `misc_ip.md` - reusable support RTL blocks
