## ppvcd

`ppvcd` loads a waveform from a VCD file, converts it to a PulsePins sequence, and sends it to the streamer.

It is implemented in `c++/pptool.cc` and uses the VCD parser support in `c++/vcd_parser.hh` together with the normal
streamer, trigger, readback, and counter interfaces.

The tool is intended for taking a waveform description that already exists in simulation form and replaying it through the PulsePins streamer hardware.

Common options:

* `-file PATH`: input VCD file to load
* `-target NAME`: VCD signal name to convert; defaults to `outs`
* `-scale N`: output period in ns for each VCD time unit; defaults to `10`
* `-force`: force triggering after loading the sequence

### How conversion works

The parser scans the VCD definitions, finds the requested signal by name, and then records value changes for that signal.

For each update:

* the VCD timestamp is parsed
* the timestamp is divided by the scale factor
* the resulting value/time pair is converted into a PulsePins sequence update

At the end of parsing, the converter appends a final zero-valued end event so the generated sequence has a defined terminal point.

The current implementation supports:

* vector changes of the form `b1010 <id>`
* scalar changes of the form `0<id>` and `1<id>`

### Practical use

Use `ppvcd` when a waveform is easiest to define in HDL simulation or when you already have a VCD trace from another design and want to reproduce it on the physical outputs.

If `-force` is given, the tool forces the trigger path so playback starts immediately after the sequence is loaded.
