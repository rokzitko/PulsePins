# pphelloworld

`pphelloworld` is the smallest built-in live output-toggle smoke test.

It programs a two-state repeating sequence that toggles all output bits between `0xffffffff` and `0x00000000`.

This command is intentionally broad and continuous: it drives all 32 outputs and does not stop the FPGA replay when the process exits. Use the finite [First output](manual/first_output.md) chapter for initial wiring checks.

## When to use it

Use `pphelloworld` when you want a minimal end-to-end check that:

* the basic streamer path is alive
* outputs are toggling at the expected clock-derived rate
* your scope or logic analyzer can see activity without setting up a more complex workflow

## Common options

* `-core_pll`
* `-int_pll`
* `-int_clk`

The PLL options configure the candidate clocks. Use `-int_clk` when the test must explicitly select the internal streamer-clock path; programming `-int_pll` alone does not change a previously selected external path.

## Examples

Run with the default clock settings:

```bash
pphelloworld
```

Run at an explicitly selected 10 MHz internal/core clock point:

```bash
pphelloworld -core_pll 10M -int_pll 10M -int_clk
```

## What to expect

The command loads a small sequence with two stored elements and a replay instruction, then forces triggering.

After startup, all outputs toggle with frequency:

* `streamer_clk / 1000`

At the default 100 MHz streamer clock, that is 100 kHz.

This makes `pphelloworld` useful for checking output toggling and the expected output rate on a scope.

After the test, run `ppreset -i 0x0`. The reset leaves the outputs actively driven low. Before connecting another driver, power down or release the bidirectional pins with `ppread -oe 0 -hard-timeout 100ms`.

## Related pages

* [pptest - self-tests](pptest.md)
* [ppfg - PulsePins Function Generator](ppfg.md)
* [Testing procedures](testing.md)
* [First finite output](manual/first_output.md)
