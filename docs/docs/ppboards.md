# ppboards - shields for the DE10-Nano board

ppboards are shields that plug into the two 40-pin 2.54mm GPIO pin headers on the DE10-Nano board.

## PP_PMOD

This PulsePins shield board provides output signals broken out on four dual
[PMOD](https://digilent.com/reference/_media/reference/pmod/pmod-interface-specification-1_3_1.pdf) ports carrying 8
bits each. In addition, the board has dual PMOD connectors for trigger inputs and AUX signals, and
regular PMOD connectors for clocking, trigger control and streamer status signals. Other features:

* all signals on connectors have ESD protection diodes
* SMA connectors for external clock and for pulse-per-second (PPS) signals; monitoring LED for PPS
signal; optinal built-in 50-ohm terminators
* SMA connector for one trigger signal; it is connected to a fast comparator with a tunable
reference voltage; optional 50-ohm terminator; monitoring LED
* two output signals are wired to SMA connectors with 50-ohm line drivers; monitoring LEDs 
* Status LEDs: trigger armed, trigger activated, done, buffer error
* Activity & heartbeat LEDs
* QWIIC I2C connector for external modules
* Optional temperature monitor (I2C interface)
* Optional 16-bit DAC (I2C interface) with separate low-noise power supply (possible application
is frequency tuning of OCXO; in combination with PPS input this can be turned into a simple GPSDO)
* Testpoints for troubleshooting

KiCAD schematics and PCB, as well as the gerber files for producing the boards, are available here.
