// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Predefined symbolic PLL settings.
//
// These aliases provide stable, human-friendly names for commonly used `(N,M,C)` triplets
// so CLI users and higher-level tools can request frequencies without spelling out raw PLL
// parameters every time.

#pragma once

#include <string>
#include <vector>
#include <utility>

inline std::string applyReplacement(const std::string &s,
                                    const std::vector<std::pair<std::string, std::string>> &rules)
{
  for (const auto &[from, to] : rules)
    if (s == from)
      return to; // full-string match -> replace
  return s; // no replacement applied
}

// N,M,C triplets, freq = ref * M / (N * C), where ref = 50 MHz on DE10-Nano.
static const std::vector<std::pair<std::string, std::string>> pll_rules = {
  {"100M", "5,20,2"},
  {"80M",  "3,24,5"},
  {"75M",  "5,30,4"},
  {"60M",  "5,30,5"},
  {"50M",  "5,30,6"},
  {"40M",  "5,20,5"},
  {"25M",  "10,30,6"},
  {"30M",  "10,30,5"},
  {"20M",  "10,20,5"},
  {"10M",  "10,20,10"},
  {"5M",   "20,20,10"},
  {"1M",   "50,20,20"},
  {"100k", "100,20,100"},
  {"10k",  "500,20,200"},
  {"lj",   "1,20,10"},   // 100MHz, low jitter
  {"ilj",  "1,17,13"},   // 65.3846 MHz, incommensurate low jitter
  {"ih",   "3,71,13"},   // 91.0256 MHz
  {"il",   "5,79,17"},   // 46.4706 MHz
  {"i2h",  "7,223,17"},  // 93.6975 MHz
  {"i2l",  "9,271,23"}   // 65.4589 MHz
};

// Ratio ih/il is 6035/3081 ~ 1.95878.
// Ratio i2h/i2l is 46161/32249 (see pll_calc.nb).
// These combinations represent an approximant for an incommensurate frequency ratio.
