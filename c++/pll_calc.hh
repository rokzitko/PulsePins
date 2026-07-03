// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Pure Cyclone V integer-PLL parameter calculator for the PulsePins 50 MHz reference clock.

#pragma once

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "freqfmt.hh"
#include "misc.hh"

namespace pllcalc {

constexpr double reference_hz = 50.0e6;
constexpr double pfd_min_hz = 5.0e6;
constexpr double pfd_max_hz = 325.0e6;
constexpr double vco_min_hz = 600.0e6;
constexpr double vco_max_hz = 1600.0e6;
constexpr double preferred_vco_hz = 1000.0e6;
constexpr int counter_min = 1;
// The current reconfiguration helper represents high/low counter fields as uint8_t values
// and encodes each half directly, so calculated profiles are limited to the safe field range.
constexpr int counter_max = 510;
constexpr double output_min_hz = vco_min_hz / counter_max;
constexpr double output_max_hz = vco_max_hz / counter_min;

struct PllParameters {
  int n = 0;
  int m = 0;
  int c = 0;
  double requested_hz = 0.0;
  double actual_hz = 0.0;
  double pfd_hz = 0.0;
  double vco_hz = 0.0;
  double error_hz = 0.0;
  double error_ppm = 0.0;

  std::string config_string() const {
    std::ostringstream out;
    out << n << ',' << m << ',' << c;
    return out.str();
  }
};

struct PllProfileResolution {
  std::string config;
  std::optional<PllParameters> calculated;
};

inline std::string format_frequency_hz(const double hz) {
  return freqfmt::format_frequency(hz, 10, '_', true);
}

inline bool parse_raw_config(const std::string &profile, int *n = nullptr, int *m = nullptr, int *c = nullptr) {
  std::istringstream iss(profile);
  int pn = 0;
  int pm = 0;
  int pc = 0;
  char comma1 = '\0';
  char comma2 = '\0';
  if (!(iss >> pn >> comma1 >> pm >> comma2 >> pc) || comma1 != ',' || comma2 != ',') {
    return false;
  }
  iss >> std::ws;
  if (!iss.eof()) {
    return false;
  }
  if (n) *n = pn;
  if (m) *m = pm;
  if (c) *c = pc;
  return true;
}

inline std::optional<double> parse_frequency_hz(const std::string &profile) {
  try {
    const double hz = parse_frequency(profile);
    if (hz > 0.0)
      return hz;
    return std::nullopt;
  }
  catch (const std::exception &) {
  }

  std::string number;
  std::string unit;
  split_number_unit(profile, number, unit);
  if (number.empty() || unit.empty()) {
    return std::nullopt;
  }

  double value = 0.0;
  try {
    value = parse_strict_finite_double(number, "frequency value");
  } catch (const std::exception &) {
    return std::nullopt;
  }
  if (value <= 0.0) {
    return std::nullopt;
  }

  double scale = 0.0;
  if (unit == "M") {
    scale = 1.0e6;
  } else {
    for (auto &ch : unit)
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (unit == "g") scale = 1.0e9;
    if (unit == "k") scale = 1.0e3;
    if (unit == "m") scale = 1.0e-3;
    if (unit == "u") scale = 1.0e-6;
  }
  if (scale == 0.0) {
    return std::nullopt;
  }
  return value * scale;
}

inline PllParameters make_parameters(const int n, const int m, const int c, const double requested_hz) {
  PllParameters params;
  params.n = n;
  params.m = m;
  params.c = c;
  params.requested_hz = requested_hz;
  params.pfd_hz = reference_hz / n;
  params.vco_hz = params.pfd_hz * m;
  params.actual_hz = params.vco_hz / c;
  params.error_hz = params.actual_hz - requested_hz;
  params.error_ppm = params.error_hz / requested_hz * 1.0e6;
  return params;
}

inline bool is_strict_candidate(const int n, const int m, const int c) {
  if (n < counter_min || n > counter_max) return false;
  if (m < counter_min || m > counter_max) return false;
  if (c < counter_min || c > counter_max) return false;
  const double pfd = reference_hz / n;
  const double vco = pfd * m;
  return pfd >= pfd_min_hz && pfd <= pfd_max_hz &&
    vco >= vco_min_hz && vco <= vco_max_hz;
}

inline bool is_better(const PllParameters &candidate, const PllParameters &best) {
  constexpr double eps_hz = 1.0e-9;
  const double candidate_error = std::fabs(candidate.error_hz);
  const double best_error = std::fabs(best.error_hz);
  if (candidate_error < best_error - eps_hz) return true;
  if (candidate_error > best_error + eps_hz) return false;

  if (candidate.n != best.n) return candidate.n < best.n;

  const double candidate_vco_delta = std::fabs(candidate.vco_hz - preferred_vco_hz);
  const double best_vco_delta = std::fabs(best.vco_hz - preferred_vco_hz);
  if (candidate_vco_delta < best_vco_delta - eps_hz) return true;
  if (candidate_vco_delta > best_vco_delta + eps_hz) return false;

  if (candidate.m != best.m) return candidate.m < best.m;
  return candidate.c < best.c;
}

inline std::optional<PllParameters> calculate(const double requested_hz) {
  if (requested_hz < output_min_hz || requested_hz > output_max_hz) {
    return std::nullopt;
  }

  std::optional<PllParameters> best;
  for (int n = counter_min; n <= counter_max; ++n) {
    const double pfd = reference_hz / n;
    if (pfd < pfd_min_hz || pfd > pfd_max_hz) continue;

    for (int m = counter_min; m <= counter_max; ++m) {
      const double vco = pfd * m;
      if (vco < vco_min_hz) continue;
      if (vco > vco_max_hz) break;

      for (int c = counter_min; c <= counter_max; ++c) {
        const auto candidate = make_parameters(n, m, c, requested_hz);
        if (!best || is_better(candidate, *best)) {
          best = candidate;
        }
      }
    }
  }
  return best;
}

inline std::optional<PllParameters> calculate(const std::string &profile) {
  const auto requested_hz = parse_frequency_hz(profile);
  if (!requested_hz) {
    return std::nullopt;
  }
  return calculate(*requested_hz);
}

inline PllProfileResolution resolve_profile(const std::string &requested_profile,
                                            const std::string &preset_resolved_profile) {
  if (preset_resolved_profile.empty()) {
    return {preset_resolved_profile, std::nullopt};
  }

  int n = 0;
  int m = 0;
  int c = 0;
  if (parse_raw_config(preset_resolved_profile, &n, &m, &c)) {
    return {preset_resolved_profile, std::nullopt};
  }

  const auto calculated = calculate(requested_profile);
  if (!calculated) {
    throw std::runtime_error("Invalid PLL profile or frequency: '" + requested_profile + "'");
  }
  return {calculated->config_string(), calculated};
}

} // namespace pllcalc
