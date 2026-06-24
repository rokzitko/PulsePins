# ppaux

`ppaux` samples the AUX input port and prints formatted readings.

It is a simple observation/debugging tool for the 8-bit auxiliary input path.

## When to use it

Use `ppaux` when you want to:

* verify AUX wiring
* log slow-changing auxiliary states
* capture a quick digital trace without using the full readback path

## Common options

* `-nr`: number of samples to read; `0` means run forever
* `-wait`: delay between samples in seconds
* `-mode`: output formatting, for example `hex`, `bin`, `dec`, or combined forms such as `hex:bin:dec`
* `-file`: write to a file instead of standard output
* `-ctr`: prefix each line with a 1-based sample counter
* `-ts`: prefix each line with an [ISO 8601](https://en.wikipedia.org/wiki/ISO_8601) timestamp

## Examples

Read ten samples using the default formatting:

```bash
ppaux -nr 10
```

Read continuously every 100 ms with timestamps and counters:

```bash
ppaux -nr 0 -wait 0.1 -mode hex:bin -ctr -ts
```

Save 100 decimal samples to a file:

```bash
ppaux -nr 100 -mode dec -file aux_capture.txt
```

## What to expect

Each output line contains the sampled AUX value formatted according to `-mode`.

If `-ctr` is present, the sample counter is prepended.
If `-ts` is present, the timestamp is prepended.
If `-file` is present, the output goes to the file instead of the terminal.

## Related pages

* [ppcounter](ppcounter.md)
* [ppts](ppts.md)
