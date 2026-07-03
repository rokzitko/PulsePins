# pllcalc

`pllcalc` computes integer [Cyclone V](https://www.intel.com/content/www/us/en/products/details/fpga/cyclone/v.html) [phase-locked loop](https://en.wikipedia.org/wiki/Phase-locked_loop) (PLL) parameters for the PulsePins 50 MHz reference clock without touching the FPGA hardware.

## Usage

```bash
pllcalc 66M
```

The frequency argument may use the usual frequency suffixes such as `Hz`, `kHz`, `MHz`, and `GHz`. Bare `M`, `k`, and `G` suffixes are also accepted for convenience, so `66M` means 66 MHz.

The command prints the requested frequency, the computed `N,M,C` triplet, the actual output frequency, phase-frequency detector frequency (`fPFD`), VCO frequency (`fVCO`), and the residual error.

## Constraints

Calculated settings follow the Cyclone V integer-PLL limits used by the project:

* `fPFD` must be between 5 MHz and 325 MHz
* `fVCO` must be between 600 MHz and 1600 MHz for the DE10-Nano `-I7` speed grade
* generated `N`, `M`, and `C` values must be programmable by the PLL reconfiguration helper

If no strict solution exists, `pllcalc` exits nonzero and does not print a substitute unsafe configuration.

## CLI PLL Options

The normal `-core_pll` and `-int_pll` options first examine existing preset names from
[`c++/pll_rules.hh`]({{ source_file("c++/pll_rules.hh") }}) and raw `N,M,C` strings. Raw triplets are accepted only when they satisfy the same strict `fPFD` and `fVCO` limits listed above. If the value is neither a preset nor raw parameters,
the string is parsed as a requested frequency and the PLL calculator is used to obtain the N,M,C
parameters.

When runtime calculation is used, the tool prints a warning and the calculated parameters to standard output before programming the PLL.
