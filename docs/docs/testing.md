# Tesing procedures

PulsePins has been designed from the start to allow a high degree of internal self-testing. For
this reason, the FPGA fabric includes a run-length-encoder for reading back the generated stream
and performing comparisons against the expected results. These tests are performed with the
[pptest](pptest.md) tool. A collection of shell scripts can be found in the tests/ subdirectory.
To start one sweep of the test battery, run ``run_all_tests``. If any error is encountered, the
testing is immediately terminated. A successful completion is reported by printing a message
``SUCCESS.`` in green.

One can run the tests continuously using ``run_all_tests_forever`` for performing intensive
stress testing. The results are collected in /var/volatile for inspection.

## Wiring up for testing

Recommended color coding for connecting DE10-Nano to Saleae logic analyzer is provided in
the form of comments in ``pulsepins.sv`` design file.

The image below shows a testing jig. It makes use of custom cables to connect the logic
analyzer to both GPIO ports. Eight bits of qout on the Arduino port are connected to LEDs.
An oscilloscope is connected to clock signals on the Arduino port. Some  wires for hooking up
external clock sources are also visible.

![PulsePins testing jig](img/testing_jig.jpeg){: style="width:800px"}

## Arduino header

Some signals are additionally brought out on the Arduino header for testing purposes.

| Port   | Name |
| ------ | ---- |
| D[7:0] | streamer_qout[7:0] |
| D8     | streamer_clk (internal or external, as configures) |
| D9     | core_clk (PLL output) |
| D10    | int_clk (PLL output) |
| D11    | streamer_qout[0] |
| D12    | streamer_qout[1] |


## Manual testing

Some test procedures required additional external testing equipement or external wiring.

### Multiboard

### Random number generator

The built-in rundom number generator can be tested by running ``pptest 19``. By connecting
any of the output pins to a spectrum analyzer, one can examine the whiteness of the spectrum.
(For SAs with 50ohm input impedance, one can use the 50ohm output driver of the [PP_PMOD
board](ppboards.md)).
