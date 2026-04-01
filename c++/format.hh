// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Small parsing and formatting helpers for command-line text values.

#pragma once

#include <string>
#include <string_view>
#include <system_error>  // std::errc
#include <optional>
#include <cstdlib> // std::strtol
#include <cerrno>
#include <climits>
#include <iomanip>
#include <sstream>

#include "misc.hh"

inline std::optional<int> to_int(std::string_view sv) {
  std::string s(sv);
  char* end = nullptr;
  errno = 0;
  long value = std::strtol(s.c_str(), &end, 10);
  if (end == s.c_str() || *end != '\0' || errno == ERANGE ||
      value < INT_MIN || value > INT_MAX) {
    return std::nullopt;
  }
  return static_cast<int>(value);
}

inline std::string setw_l(std::string s, std::string_view w) {
  std::stringstream ss;
  ss << std::setw(to_int(w).value_or(0)) << std::left << s;
  return ss.str();
}

// uint32_t as 0x00112233, zero padded
inline std::string hex8(uint32_t x) {
  std::stringstream ss;
  ss << "0x" << std::setw(8) << std::setfill('0') << std::hex << x;
  return ss.str();
}

// uint32_t as 1_234_567_890, right aligned
inline std::string dec13(uint32_t x) {
  std::stringstream ss;
  ss << std::setw(13) << std::setfill(' ') << with_underscores(x);
  return ss.str();
}
