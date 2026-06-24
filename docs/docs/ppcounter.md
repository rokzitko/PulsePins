# ppcounter

`ppcounter` exercises the event-counter logic and reports captured counter values.

It is primarily a measurement and self-test tool for the integrated counter subsystem rather than a generic waveform tool.

## When to use it

Use `ppcounter` when you want to:

* validate the integrated counter subsystem
* inspect counter statistics after a known reference sequence
* compare deterministic and pseudo-random counter behavior

For broader streamer and playback validation, use [`pptest`](pptest.md).

## Common modes

* `-test1`: run the first built-in deterministic counter test sequence
* `-test2`: run the second built-in pseudo-random counter test sequence
* `-check`: validate the expected result of `-test1`
* `-c`: repetition/count argument used by `-test2`

## Examples

Run the short deterministic test sequence and print the reports:

```bash
ppcounter -test1
```

Run the same deterministic sequence and check the expected counters:

```bash
ppcounter -test1 -check
```

Run the longer pseudo-random test sequence with 1000 generated values:

```bash
ppcounter -test2 -c 1000
```

## What to expect

The command performs the same high-level flow in all modes:

1. reset the counter bank
2. optionally generate a built-in test sequence
3. stream it to the FPGA
4. force execution
5. latch all counter instruments
6. print the reports

The report typically includes:

* basic statistics
* run-length statistics
* packet statistics
* short-sequence histogramming
* autocorrelation, and optionally cross-correlation if enabled in the build

## Related pages

* [Counter subsystem](counter.md)
* [pptest - self-tests](pptest.md)
* [`ip/counter/README.md`]({{ source_file("ip/counter/README.md") }})
