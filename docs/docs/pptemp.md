## pptemp

`pptemp` reads temperature samples from an MCP9808 I2C temperature sensor and prints them periodically.

The implementation is in `c++/pptool_measurement.cc` and uses the MCP9808 helper in `c++/MCP9808.hh`.

### Syntax

```bash
pptemp [options]
```

### Options

* `-bus N`: I2C bus number, default `1` for `/dev/i2c-1`
* `-addr A`: 7-bit I2C address, default `0x18`; decimal and hexadecimal values are accepted
* `-wait T`: interval between samples, default `1s`; time units such as `ms`, `s`, `min`, and `h` are accepted
* `-nr N`: number of samples to print, default `0`; `0` means run forever
* `-timestamp`: prefix human-readable output lines with an ISO-8601 UTC timestamp
* `-csv`: print CSV output with a timestamp column and one column for each enabled temperature unit
* `-celsius`: enable Celsius output; this is the default and is accepted for explicit scripts
* `-fahrenheit`: also output Fahrenheit
* `-kelvin`: also output Kelvin
* `-reopen`: reopen the I2C bus for each sample instead of keeping it open
* `-quiet-errors`: on I2C read errors, print a placeholder/error line and continue instead of exiting

### Runtime Behavior

By default, `pptemp` reads `/dev/i2c-1` at address `0x18`, samples once per second, runs indefinitely, and prints human-readable Celsius output.

For each sample the tool:

1. reads register `0x05` from the MCP9808 over I2C
2. decodes the raw value into degrees Celsius
3. formats the result as one output line
4. sleeps for the configured interval before the next sample

CSV output always includes a timestamp column. With the default unit set, the header is:

```text
timestamp,temp_c
```

With all units enabled, the header is:

```text
timestamp,temp_c,temp_f,temp_k
```

### Examples

Run continuously with default Celsius output:

```bash
pptemp
```

Print 10 timestamped samples at 500 ms intervals:

```bash
pptemp -timestamp -wait 500ms -nr 10
```

Print CSV with Celsius, Fahrenheit, and Kelvin:

```bash
pptemp -csv -fahrenheit -kelvin
```

Use a different I2C bus and address:

```bash
pptemp -bus 0 -addr 0x19 -nr 1
```

Log CSV data and keep going across transient I2C read errors:

```bash
pptemp -csv -quiet-errors >> temp.csv
```

See also [PP_PMOD hardware reference](pp_pmod_reference.md).
