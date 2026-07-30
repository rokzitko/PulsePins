# ppreset

`ppreset` pulses the FPGA S2F reset and resets streamer 1 through the normal single-streamer bring-up path.

Use it when you want to return the primary streamer to a known idle state after an aborted run, an infinite/retriggered sequence, a manual trigger experiment, or a command that intentionally skipped normal cleanup.

## Syntax

```bash
ppreset [options]
```

## What It Does

`ppreset` uses the same shared startup path as the other `pptool`-family commands, then constructs the standard single-streamer helper.

The command:

* applies any requested clock or PLL changes during the normal startup sequence
* always performs an FPGA S2F reset during startup, equivalent to passing `-reset_FPGA`
* programs the streamer initial output value, defaulting to `0`
* asserts the physical output enable (`oe`)
* restores the streamer's software-visible control word to its persistent defaults
* pulses the streamer reset bit

This is a streamer/runtime reset, not a bitstream reload. It does not reprogram `pulsepins.rbf`, and it does not directly reset readback or counter state.

`ppreset` is not a high-impedance operation: it asserts physical output enable and actively drives the selected `-i` value. To stop the streamer at zero and then release the bidirectional `qout`, `qout_valid`, and `qout_strobe` pins before connecting another driver, use:

```bash
ppreset -i 0x0
ppread -oe 0 -hard-timeout 100ms
```

The bounded `ppread` invocation changes `oe` to input mode and exits after the hard timeout; power-down is an alternative when changing wiring.

## Common Options

Streamer reset options:

* `-i VALUE`: set the idle/initial output value used after reset

Shared startup options that are often relevant before a reset:

* `-core_pll PROFILE`: configure `core_clk`
* `-int_pll PROFILE`: configure the internal candidate streamer clock
* `-int_clk`: select the internal streamer clock path
* `-ext_clk`: select the external streamer clock path
* `-clk N`: select a raw clock mux value
* `-reset_FPGA`: accepted for consistency; `ppreset` performs this reset even when the option is omitted
* `-dark_mode`: disable status LEDs during startup

## Expected Output

On success, `ppreset` prints the usual startup/version and frequency-meter information, then exits with the shared success return code:

```text
All done, exiting with return code 0
```

If `-i` is set to a non-zero value, the command also reports the configured initial value before pulsing reset.

## Troubleshooting

Use `ppreset` when trigger force/enable state, qout override state, or streamer status appears stale after manual experiments.

If the streamer still does not return to a usable state, reload the FPGA image with `FPGA-writeConfig -f pulsepins.rbf`.

If outputs come up at the wrong idle value, pass the intended pattern explicitly with `-i VALUE`.

For follow-up diagnosis, use:

* [`pptrig - trigger debugging tool`](pptrig.md)
* [`ppqout - output combiner inspection`](ppqout.md)
* [`pphelloworld - quick output smoke test`](pphelloworld.md)
* [`pptest - self-tests`](pptest.md)
* [Testing procedures](testing.md)
