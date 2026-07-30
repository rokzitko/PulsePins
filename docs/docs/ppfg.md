# ppfg - PulsePins Function Generator

ppfg is a simple "function generator" for binary signals.

Command line switches:

* ``-period``: time period (floating point value); the unit can be specified attached to the
number with no space (accepted suffixes are: ns; us; ms; s=sec=secs; min; h=hr=hrs; d=day=days, u
stands for micro, case insensitive; for example 20ms), default unit is seconds; no default value
* ``-freq``: frequency (floating point value); the unit can be specified attached to the number with
no space (accepted suffixes are: uHz, mHz, Hz, kHz, MHz, GHz, u stands for micro, case insensitive
except for m/M; for example 5MHz), default unit is Hz; no default value
* ``-duty``: duty cycle as a percentage of the 'on' signal; fractional values are allowed (floating point);
default value is 50
* ``-servo``: angle setting for testing [servo motors](#servo-motors) using pulse-width modulation (PWM);
the angle argument is required when the switch is used
* ``-v0``: output pattern for low state (default: ``0x00000000``)
* ``-v1``: output pattern for high state (default: ``0xFFFFFFFF``)
* ``-start0``: set the first pattern presented on the output to the low state (instead of high state; default: off)
* ``-delay``: delay after trigger before generating the output sequence (in seconds, same syntax as for
period; default: ``0``)
* ``-p``: trigger pattern (default: ``0b00000001``)
* ``-m``: trigger mask (default: ``0b00000001``)
* ``-i``: initial output value before triggering (default: ``0``)
* ``-trig``, ``-autotrig``: do not wait for external trigger, automatically start (default: off)
* ``-gate``: gate settings, specified by a [gate string](#gate-settings) (default: disabled)
* ``-gate_debug``: start the program in gate debug mode; gate signal state is dumped
every 100 ms (default: off)
* ``-core_pll``, ``-core_pll_charge_pump``, ``-core_pll_bandwidth``: core-clock PLL settings, as explained [here](pptest.md#pll-settings)
* ``-int_pll``, ``-int_pll_charge_pump``, ``-int_pll_bandwidth``: internal streamer-clock PLL settings, as explained [here](pptest.md#pll-settings)

Specify one positive waveform timing source with ``-period`` or ``-freq``; these two options are mutually exclusive.
In servo mode, ``-servo`` supplies the pulse width from the requested angle. If neither ``-period`` nor
``-freq`` is supplied, servo mode uses the standard 20 ms / 50 Hz servo PWM period. If ``-period`` or
``-freq`` is supplied together with ``-servo``, that explicit timing is used and the duty cycle is
recomputed from the servo pulse width. The explicit period must not be shorter than that width; use a
longer period when a nonzero low phase is required.

There are two modes of operation, burst mode and continuous mode.

The default high value is `0xFFFFFFFF`, which drives all 32 output bits high. User-facing procedures should set `-v1`, `-v0`, and the final value explicitly. Start with the finite [`qout[0]` manual chapter](manual/first_output.md) rather than relying on these defaults.

## Gate settings

The gate controls whether streamer output is allowed to advance and is configured with a 'gate string': `-gate en:gate_in_en:mask`.

* en: enable is a boolean (t=T=1, f=F=0)
* gate_in_en: enable dedicated gate input (gate_in)
* mask: trigger mask (default: ``0b00000000``); signals on the trigger port can also be used to
open the gate and allow output advancement

## Burst mode

Activated with ``-burst N``. The parameters are:

* ``-burst``: number of pulse repetitions after activation; the argument is required when burst mode is used
* ``-t``: final value (default: ``0``)
* ``-n_max``: maximum number of bursts (default: ``1``; ``0`` means infinity)

`-burst 0` also encodes infinite replay in the FPGA, so use strictly positive `-burst` and `-n_max` values for a finite run. With `-trig` or `-autotrig`, a positive finite burst is forced after queueing; without either option, it is only armed for the selected trigger condition. In both cases the command returns without waiting for hardware completion. An untriggered burst can therefore remain latent indefinitely, and shell-prompt return does not mean the final value has been reached.

## Continuous mode

Activated with ``-cont`` (default: off).

Continuous mode keeps the foreground process paused after it queues and either forces or arms the sequence. Interrupting that process does not itself clear repeating or armed hardware state.

## Stopping infinite or armed output

`-burst 0` returns to the shell even though its replay becomes infinite after activation and never reaches the queued final value. `-n_max 0` keeps the host in an infinite enqueue loop until interruption or transport failure. Activated continuous mode is also unbounded, while any mode without `-trig` or `-autotrig` can remain armed. Interrupting either foreground loop does not clear hardware state. Run `ppreset -i 0x0` to cancel these states; this returns the bus to zero but leaves the physical output drivers enabled. Before connecting another driver, power down or release the bidirectional pins with `ppread -oe 0 -hard-timeout 100ms`.

## Servo motors

As a convenience, using the ``-servo`` switch, ppfg can generate an appropriate pulse-width modulation
signal for testing servo motors.

By default, ``-servo`` uses a 20 ms / 50 Hz PWM period and maps angles from 0 to 180 degrees onto 1 ms
to 2 ms high pulses. For example, ``-servo 90`` produces a 1.5 ms high pulse, i.e. 7.5% duty cycle at
the default 20 ms period.

If ``-period`` or ``-freq`` is also supplied, ppfg derives duty from the servo pulse width and the
explicit period. For example, ``-servo 90 -freq 100Hz`` uses a 10 ms period with a requested 1.5 ms
high pulse, i.e. 15% duty cycle before clock-count quantization. A period shorter than the requested
servo pulse yields a duty above 100% and is rejected.

In servo mode the duty cycle is derived from the angle, so ``-duty`` is ignored.

The servo option only generates the digital waveform. It does not define a safe servo power supply, ground connection, level interface, or current path; document those separately for the connected hardware.
