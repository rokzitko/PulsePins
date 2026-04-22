# ppqout

`ppqout` is the low-level output-combiner inspection and testing tool.

It is mainly used to verify how the four streamer outputs are routed, masked, inverted, forced, and combined into the final `qout` bus.

## When to use it

Use `ppqout` when you want to:

* inspect or change the combiner mode directly
* verify masks, inversions, and forced values without streaming a full sequence
* run the combiner self-tests
* debug streamer routing before bringing up a peripheral or external instrument

## Common options

Streamer values:

* `-q1`, `-q2`, `-q3`, `-q4`: set the direct qout value for each streamer

Combiner mode:

* `-out_sel1`, `-out_sel2`, `-out_sel3`, `-out_sel4`
* `-out_and`, `-out_or`, `-out_xor`, `-out_xnor`
* `-out_maj`
* `-out_block8`, `-out_block16`
* `-out_sum12`, `-out_sum1234`, `-out_diff12`

Per-port transforms:

* `-invert1`, `-invert2`, `-invert3`, `-invert4`
* `-mask1`, `-mask2`, `-mask3`, `-mask4`
* `-force1`, `-force2`, `-force3`, `-force4`

Output transforms:

* `-invert_out`
* `-mask_out`
* `-force_out`

Reporting and tests:

* `-report_pre`
* `-report_post`
* `-self_test`
* `-test <n>`

## Examples

Select streamer 1 directly:

```bash
ppqout -veryverbose -out_sel1
```

Run the built-in combiner self-test:

```bash
ppqout -self_test
```

Stress-test the combiner path:

```bash
ppqout -test 100000
```

Inspect a specific mask/invert/force configuration:

```bash
ppqout -veryverbose \
  -invert1 0x12345678 -invert2 0xabcdef12 -invert3 0x11223344 -invert4 0x44332211 \
  -mask1 0xffffff00 -mask2 0xffff00ff -mask3 0xff00ffff -mask4 0x00ffffff \
  -invert_out 0x1 -mask_out 0xffffffee \
  -q1 0xaabb11dd -q2 0xaa22ccdd -q3 0x33abcdef -q4 0x44444444 \
  -report_pre -report_post
```

## What to expect

With `-veryverbose`, `ppqout` prints:

* the selected combiner mode
* the configured inversion, mask, and force values for each input and the output
* the live `qout(streamer N)` values
* the transformed `combiner_inN` values
* the resulting `combiner_out`

`-self_test` performs quick built-in checks and returns a non-zero exit status if a failure is detected.
`-test <n>` runs a more intensive randomized combiner validation loop.

## Related pages

* [`combiner.md`](combiner.md)
* [`pptrig.md`](pptrig.md)
* [`pp_pmod_reference.md`](pp_pmod_reference.md)
