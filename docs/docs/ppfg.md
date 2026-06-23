# ppfg - PulsePins Function Generator

ppfg is a simple "function generator" for binary signals.

Command line switches:

* ``-period``: time period (floating point value); the unit can be specified attached to the
number with no space (accepted suffixes are: ns; us; ms; s=sec=secs; min; h=hr=hrs; d=day=days, u
stands for micro, case insensitive; for example 20ms), default unit is seconds
* ``-freq``: frequency (floating point value); the unit can be specified attached to the number with
no space (accepted suffixes are: uHz, mHz, Hz, kHz, MHz, GHz, u stands for micro, case insensitive
except for m/M; for example 5MHz), default unit is Hz
* ``-duty``: duty cycle as percentage of 'on' signal, fractional values allows (floating point);
default value is 50
* ``-servo``: angle setting for testing [servo motors](#servo-motors) using pulse-width modulation (PWM); default
value is 90 degrees
* ``-v0``: output pattern for low state
* ``-v1``: output pattern for high state
* ``-start0``: set the first pattern to be presented on output the low state (instead of high state)
* ``-delay``: delay after trigger before generating the output sequence (in seconds, same syntax as for
period)
* ``-p``: trigger pattern (default is b00000001)
* ``-m``: trigger mask (default is b00000001)
* ``-i``: initial output value (before triggering)
* ``-trig``, ``-autotrig``: do not wait for external trigger, automatically start
* ``-gate``: gate settings, specified by a [gate string](#gate-settings)
* ``-gate_debug``: start the program in gate debug mode; gate signal state is dumped
every 100ms
* ``-core_pll``, ``-core_pll_charge_pump``, ``-core_pll_bandwidth``: core-clock PLL settings, as explained [here](pptest.md#pll-settings)
* ``-int_pll``, ``-int_pll_charge_pump``, ``-int_pll_bandwidth``: internal streamer-clock PLL settings, as explained [here](pptest.md#pll-settings)

There are two modes of operation, burst mode and continuous mode.

## Gate settings

The gating can be enabled through a specification in a 'gate string': `-gate en:gate_in_en:mask`.

* en: enable is a boolean (t=T=1, f=F=0)
* gate_in_en: enable dedicated gate input (gate_in)
* mask: trigger mask (default is b00000000); signals on the trigger port can also be used to
enable gate

## Burst mode

Activated with ``-burst``. The parameters are:

* ``-burst``: number of pulse repetitions after activation; default is 1
* ``-t``: final value
* ``-n_max``: maximum number of bursts; default is 1, 0 means infinity

## Continuous mode

Activated with ``-cont``.

## Servo motors

As a convenience, using the ``-servo`` switch ppfg can generate appropriate pulse-width modulation
signal for testing servo motors.
