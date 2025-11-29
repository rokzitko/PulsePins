## ppread

ppread is a tool for reading data using runlength-encoding compression.

Command line arguments:

* ``-oe``: output enable (bool). If true, we are reading internally generated data. If false, we are
reading external data on the device I/O pins. If unspecified, use the hardware default (false).
* ``-timeout``: timeout in seconds (floating point number). If positive, interpreted as time after
the last element read. If negative, interpreted as time after starting the tool.
