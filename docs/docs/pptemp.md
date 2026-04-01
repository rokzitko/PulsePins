## pptemp

`pptemp` reads temperature samples from the supported I2C temperature sensor and prints them periodically.

The current implementation is in `c++/pptool.cc` and uses the MCP9808 helper in `c++/MCP9808.hh`.

Typical usage is continuous monitoring.

### Current runtime behavior

In the current `pptool.cc` implementation, `pptemp` constructs a default `Args` object and uses it directly. That means the active runtime behavior is currently fixed to the built-in defaults shown below.

The current helper defaults are:

* I2C bus `1`
* sensor address `0x18`
* one sample per second
* unlimited sample count

In the current code path, the default `Args` values mean the output is human-readable Celsius text rather than CSV.

### Output model

For each sample the tool:

1. reads register `0x05` from the MCP9808 over I2C
2. decodes the raw value into degrees Celsius
3. formats the result as one output line
4. sleeps for the configured delay before the next sample

The underlying helper can also generate timestamped, CSV, and Fahrenheit-augmented output, but those behaviors are not currently configured by `pptool.cc`.

The underlying helper supports both human-readable and CSV-oriented output and can optionally include:

* UTC timestamps
* Fahrenheit conversion in addition to Celsius
* reopen-per-sample behavior for more defensive I2C access
* quiet error handling with placeholder output

This makes `pptemp` useful for simple interactive monitoring and plain-text logging.

### Typical examples

Run continuously:

```bash
pptemp
```

Pipe into a log file:

```bash
pptemp >> temp.log
```

See also [PP_PMOD I2C and onboard peripherals](pp_pmod_i2c.md).
