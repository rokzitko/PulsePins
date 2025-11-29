// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

//  Predefined settings for PLLs

#pragma once

#include <string>
#include <vector>
#include <utility>

std::string applyReplacement(const std::string &s,
                             const std::vector<std::pair<std::string, std::string>> &rules)
{
  for (const auto &[from, to] : rules)
    if (s == from)
      return to; // full-string match -> replace
  return s; // no replacement applied
}

// N,M,C triplets, freq=ref*M/(N*C), where ref=50MHz on DE10-Nano
std::vector<std::pair<std::string, std::string>> pll_rules = {
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
  {"10k",  "500,20,200"}
};
