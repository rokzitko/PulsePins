## PulsePin development

Users are encoureged to participate in further development of PulsePins by contributing code or by making (reasonable)
feature requests. This text documents some development practices and standards followed.

### Coding standards

Use structured and clear code. Uniform and self-describing variable names. Ample in-line documentation. Comments should explain intention. Avoid hard-coded parameters. Unit tests with high coverage, including corner cases.

### TO DO list symbols

I like to use the following symbols to annotated my TO DO lists:

 - ``!`` important, high-impact item
 - ``@`` complex task, might be time consuming or technically demanding
 - ``o`` recurring tasks, to be performed periodically (keep them on the list)
 - ``#`` major milestones, enabler for other tasks
 - ``-`` easy task
 - ``x`` low-impact, low-priority

### Source code layout

    ip/              # Verilog descriptions of circuitry
      combiner/      # IP for the advanced multiplexer
      streamer/      # IP for the RTE decoder engine
      rl_encoder_if/ # IP for the RTE encoder engine
      tagger/        # IP for the time tagger
    c++/             # C++ source code for API, pptool
    python/          # Python binding


### Documentation

These manual pages have been created using [MkDocs](https://www.mkdocs.org/). For testing the generated web site, we use
[caddy](https://caddyserver.com/).
