## ppaux

Testing tool for AUX input port.

Command line arguments:

* ``-nr``: number of samples to read; 0 for infinity
* ``-wait``: time before sampling in seconds (floating point number, microsecond resolution)
* ``-mode``: formatting of the output, may include strings 'hex', 'bin' and 'dec', e.g. hex:bin:dec for all three
* ``-file``: write to file instead of standard output
* ``-ctr``: include counter (1-based)
* ``-ts``: include ISO8601 time stamp (with millisecond resolution)
