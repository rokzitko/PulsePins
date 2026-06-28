## pptrig

pptrig is a tool for troubleshooting the triggering.

Command line switches for setting the trigger:

* ``-trig_int``: use internal trigger signals (generated using a PIO under software control)
* ``-trig_ext``: use external trigger signals
* ``-trig_misc``: use trigger signals from on-board interfaces (pushbuttons, switches);
external pulse-per-second signal is also connected to this trigger input block
* ``-trig_any``: use a combination of the above (logical or, "any of")
* ``-trig_all``: require all trigger signals to be present (logical and, "all of")
* ``-invert_trig_result v``: uses v as the inversion pattern on the output of the combiner circuit
* ``-invert_int v``: uses v as the inversion pattern on the input INT
* ``-invert_ext v``: uses v as the inversion pattern on the input EXT
* ``-invert_misc v``: uses v as the inversion pattern on the input MISC
* ``-mask_int m``: uses m as the mask for input INT (only signals marked with 1 are passed through)
* ``-mask_ext m``: uses m as the mask for input EXT (only signals marked with 1 are passed through)
* ``-mask_misc m``: uses m as the mask for input MISC (only signals marked with 1 are passed through)

Diagnostic switches:

* ``-pio v``: write ``v`` to the internal trigger PIO before reading back trigger state; default is ``0``
* ``-debug``: continuously print streamer external-trigger input/control state for live debugging

See also [PP_PMOD hardware reference](pp_pmod_reference.md).
