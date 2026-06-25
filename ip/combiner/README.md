PulsePins combiner subsystem.

This directory and its sibling combiner blocks contain the late-stage routing logic used to merge, select, mask, invert, or override multiple streamer outputs and trigger groups without modifying the underlying sequences.

## Main files

- `combiner/combiner.sv` - registered output-data combiner used in the timed datapath
- `combiner_comb/combiner_comb.sv` - purely combinational output-data variant
- `combiner_trig/combiner_trig.sv` - trigger-group combiner

## Architecture overview

The combiner sits after the streamer outputs and before the final externally visible routing.

For each input path it can:

- replace the live input with a forced value
- invert selected bits
- mask selected bits

It then applies one selected combination mode and can optionally replace the final result with a forced output value.

The registered `combiner.sv` version is the normal timed datapath block. The combinational variant exists mainly for experiments and completeness when latency matters more than clocked boundaries.

## Programming model

All combiner variants expose the same small Avalon-MM model:

- one configuration register for mode plus force/readback bits
- inversion registers for each input and the output
- mask registers for each input and the output
- value registers for each input and the output

This model is mirrored on the host side by `c++/combiner.hh`, `c++/qout.hh`, and `c++/trigger.hh`.

## Reading order for maintainers

1. `combiner.sv`
2. `combiner_trig/combiner_trig.sv`
3. `combiner_comb/combiner_comb.sv`
4. `c++/combiner.hh`
5. `c++/qout.hh`
6. `c++/trigger.hh`
7. `docs/docs/combiner.md`

## Verification and related docs

- subsystem overview: `docs/docs/combiner.md`
- host-side wrappers: `c++/combiner.hh`, `c++/qout.hh`, `c++/trigger.hh`
