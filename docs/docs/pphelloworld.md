# pphelloworld

`pphelloworld` is the smallest built-in live output-toggle smoke test.

It programs a two-state repeating sequence that toggles all output bits between `0xffffffff` and `0x00000000`.

## When to use it

Use `pphelloworld` when you want a minimal end-to-end check that:

* the basic streamer path is alive
* outputs are toggling at the expected clock-derived rate
* your scope or logic analyzer can see activity without setting up a more complex workflow

## Common options

* `-core_pll`
* `-int_pll`

These select the relevant clock configuration before the test sequence is transmitted.

## Examples

Run with the default clock settings:

```bash
pphelloworld
```

Run at an explicitly selected 10 MHz internal/core clock point:

```bash
pphelloworld -core_pll 10M -int_pll 10M
```

## What to expect

The command loads a small sequence with two stored elements and a replay instruction, then forces triggering.

After startup, all outputs toggle with frequency:

* `streamer_clk / 1000`

At the default 100 MHz streamer clock, that is 100 kHz.

This makes `pphelloworld` useful as a quick scope-visible sanity check.

## Related pages

* [`pptest.md`](pptest.md)
* [`ppfg.md`](ppfg.md)
* [`testing.md`](testing.md)
