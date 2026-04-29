// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include <exception>
#include <iostream>

#include "pll_calc.hh"

namespace {

void usage() {
  std::cerr << "Usage: pllcalc FREQUENCY\n";
  std::cerr << "Example: pllcalc 66M\n";
}

void print_result(const pllcalc::PllParameters &params) {
  std::cout << "requested: " << pllcalc::format_frequency_hz(params.requested_hz) << '\n';
  std::cout << "N,M,C: " << params.config_string() << '\n';
  std::cout << "actual: " << pllcalc::format_frequency_hz(params.actual_hz) << '\n';
  std::cout << "fPFD: " << pllcalc::format_frequency_hz(params.pfd_hz) << '\n';
  std::cout << "fVCO: " << pllcalc::format_frequency_hz(params.vco_hz) << '\n';
  std::cout << "error: " << pllcalc::format_frequency_hz(params.error_hz) << '\n';
  std::cout << "error_ppm: " << params.error_ppm << '\n';
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 2) {
    usage();
    return 1;
  }

  const std::string arg = argv[1];
  if (arg == "-h" || arg == "--help") {
    usage();
    return 0;
  }

  try {
    const auto params = pllcalc::calculate(arg);
    if (!params) {
      std::cerr << "No strict Cyclone V integer PLL parameters for '" << arg << "'\n";
      return 1;
    }
    print_result(*params);
  }
  catch (const std::exception &e) {
    std::cerr << "Fatal: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
