// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Parser for extracting timestamped value changes from VCD files.

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cctype>
#include <istream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "config.h" // provides value_t, count_t
#include "parser.hh" // provides parseVerilogInt

struct VcdUpdate {
  value_t value;
  count_t count;
};

namespace detail {

inline std::string trim(std::string_view sv) {
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))  sv.remove_suffix(1);
  return std::string(sv);
}

inline bool starts_with(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

inline std::vector<std::string> split_ws(std::string_view sv) {
  std::vector<std::string> out;
  while (!sv.empty()) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
    if (sv.empty()) break;
    std::size_t n = 0;
    while (n < sv.size() && !std::isspace(static_cast<unsigned char>(sv[n]))) ++n;
    out.emplace_back(sv.substr(0, n));
    sv.remove_prefix(n);
  }
  return out;
}

template <typename T>
inline T checked_narrow(uint64_t x, const char* what, const std::string& context = {}) {
  static_assert(std::numeric_limits<T>::is_integer, "T must be an integer type");
  static_assert(!std::numeric_limits<T>::is_signed, "T must be an unsigned integer type");

  if (x > static_cast<uint64_t>(std::numeric_limits<T>::max())) {
    if (context.empty()) {
      throw std::runtime_error(std::string(what) + " exceeds destination type range");
    }
    throw std::runtime_error(std::string(what) + " exceeds destination type range: " + context);
  }
  return static_cast<T>(x);
}

inline count_t parse_timestamp(std::string_view t, uint32_t scale_factor) {
  const std::string s(t);
  const char* p = s.c_str();
  if (*p != '#') {
    throw std::runtime_error("Invalid VCD timestamp record: " + s);
  }
  ++p;

  char* endp = nullptr;
  unsigned long long v = std::strtoull(p, &endp, 10);
  if (endp == p || *endp != '\0') {
    throw std::runtime_error("Invalid VCD timestamp: " + s);
  }
  v = v/scale_factor;

  return checked_narrow<count_t>(static_cast<uint64_t>(v), "timestamp", s);
}

} // namespace detail

inline std::vector<VcdUpdate> parseVcdUpdates(std::istream& in, std::string_view target_name = "outs", uint32_t scale_factor = 10) {
  std::vector<VcdUpdate> updates;

  std::string target_id;
  bool in_definitions = true;
  count_t current_time = 0;

  std::string line;
  while (std::getline(in, line)) {
    const std::string t = detail::trim(line);
    if (t.empty()) continue;

    if (in_definitions) {
      if (detail::starts_with(t, "$var")) {
        // Example:
        // $var reg 32 ! outs [31:0] $end
        auto tok = detail::split_ws(t);
        if (tok.size() >= 6 && tok[0] == "$var") {
          const std::string& id   = tok[3];
          const std::string& name = tok[4];
          if (name == target_name) {
            target_id = id;
          }
        }
      } else if (t == "$enddefinitions $end") {
        in_definitions = false;
      }
      continue;
    }

    if (t[0] == '#') {
      current_time = detail::parse_timestamp(t, scale_factor);
      continue;
    }

    if (t[0] == '$') {
      continue;
    }

    if (target_id.empty()) {
      continue;
    }

    // Vector change: b1010 !
    if (t[0] == 'b' || t[0] == 'B') {
      auto tok = detail::split_ws(t);
      if (tok.size() >= 2 && tok[1] == target_id) {
        const uint64_t raw = parseVerilogInt("'" + tok[0]);
        const value_t value = detail::checked_narrow<value_t>(raw, "signal value", tok[0]);
        updates.push_back(VcdUpdate{value, current_time});
      }
      continue;
    }

    // Scalar change: 0!, 1!, ...
    if (t.size() >= 2 && t.substr(1) == target_id) {
      value_t value{};
      switch (t[0]) {
      case '0': value = static_cast<value_t>(0); break;
      case '1': value = static_cast<value_t>(1); break;
      default:
        throw std::runtime_error("Unsupported scalar VCD value: " + t);
      }
      updates.push_back(VcdUpdate{value, current_time});
    }
  }

  updates.push_back(VcdUpdate{0, current_time}); // add end event

  return updates;
}
