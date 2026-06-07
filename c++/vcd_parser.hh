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
#include <utility>
#include <vector>

#include "config.h" // provides value_t, count_t
#include "parser.hh" // provides parseVerilogInt

struct VcdUpdate {
  value_t value;
  count_t count;
};

inline constexpr uint32_t default_vcd_scale_factor = 10;
inline constexpr const char *default_vcd_timescale = "10ns";

namespace detail {

struct VcdTimescale {
  uint64_t numerator_ns;
  uint64_t denominator;
};

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

inline std::string lower_ascii(std::string s) {
  for (auto &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

inline std::pair<uint64_t, std::string> split_timescale_value(std::string value) {
  value = trim(value);
  std::size_t i = 0;
  while (i < value.size() && std::isdigit(static_cast<unsigned char>(value[i])))
    ++i;
  if (i == 0 || i == value.size())
    throw std::runtime_error("Invalid VCD timescale: " + value);
  const auto multiplier = parse_strict_uint64(value.substr(0, i), "VCD timescale multiplier");
  return {multiplier, lower_ascii(value.substr(i))};
}

inline uint64_t checked_mul_u64(uint64_t a, uint64_t b, const std::string &what) {
  if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a)
    throw std::runtime_error(what + " exceeds destination type range");
  return a * b;
}

inline VcdTimescale parse_timescale(std::string_view text) {
  const auto tok = split_ws(text);
  if (tok.empty())
    throw std::runtime_error("Invalid empty VCD timescale");

  uint64_t multiplier = 0;
  std::string unit;
  if (tok.size() == 1) {
    auto parsed = split_timescale_value(tok[0]);
    multiplier = parsed.first;
    unit = parsed.second;
  } else if (tok.size() == 2) {
    multiplier = parse_strict_uint64(tok[0], "VCD timescale multiplier");
    unit = lower_ascii(tok[1]);
  } else {
    throw std::runtime_error("Invalid VCD timescale: " + std::string(text));
  }

  if (multiplier == 0)
    throw std::runtime_error("Invalid VCD timescale multiplier: " + std::string(text));

  if (unit == "s")
    return {checked_mul_u64(multiplier, 1000000000ULL, "VCD timescale"), 1};
  if (unit == "ms")
    return {checked_mul_u64(multiplier, 1000000ULL, "VCD timescale"), 1};
  if (unit == "us")
    return {checked_mul_u64(multiplier, 1000ULL, "VCD timescale"), 1};
  if (unit == "ns")
    return {multiplier, 1};
  if (unit == "ps")
    return {multiplier, 1000};
  if (unit == "fs")
    return {multiplier, 1000000};

  throw std::runtime_error("Unsupported VCD timescale unit: " + unit);
}

inline std::string strip_timescale_end(std::string text, bool &ended) {
  const auto pos = text.find("$end");
  if (pos == std::string::npos)
    return trim(text);
  ended = true;
  return trim(std::string_view(text).substr(0, pos));
}

inline uint64_t mul_div_floor_capped(uint64_t a,
                                     uint64_t b,
                                     uint64_t divisor,
                                     uint64_t limit,
                                     const std::string &context) {
  if (divisor == 0)
    throw std::runtime_error("Invalid division by zero while scaling VCD timestamp");
  if (limit == std::numeric_limits<uint64_t>::max())
    throw std::runtime_error("Invalid VCD timestamp scaling limit");

  const uint64_t overflow = limit + 1;
  uint64_t q = 0;
  uint64_t r = 0;
  uint64_t term_q = a / divisor;
  uint64_t term_r = a % divisor;

  auto add_q = [&](uint64_t add) {
    if (add >= overflow || q > limit - add) {
      q = overflow;
    } else {
      q += add;
    }
  };

  for (uint64_t bits = b; bits != 0; bits >>= 1) {
    if (bits & 1) {
      add_q(term_q);
      if (r >= divisor - term_r) {
        r = r - (divisor - term_r);
        add_q(1);
      } else {
        r += term_r;
      }
      if (q > limit)
        throw std::runtime_error("timestamp exceeds destination type range: " + context);
    }

    if ((bits >> 1) == 0)
      break;

    uint64_t carry = 0;
    if (term_r >= divisor - term_r) {
      term_r = term_r - (divisor - term_r);
      carry = 1;
    } else {
      term_r += term_r;
    }

    if (term_q >= overflow || term_q > (overflow - carry) / 2) {
      term_q = overflow;
    } else {
      term_q = term_q * 2 + carry;
    }
  }

  return q;
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

inline count_t parse_timestamp(std::string_view t, uint32_t scale_factor, VcdTimescale timescale) {
  if (scale_factor == 0) {
    throw std::runtime_error("VCD scale factor must be greater than zero");
  }
  if (timescale.denominator == 0) {
    throw std::runtime_error("Invalid VCD timescale denominator");
  }

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
  const uint64_t denominator = checked_mul_u64(scale_factor, timescale.denominator, "VCD timestamp scale divisor");
  v = mul_div_floor_capped(static_cast<uint64_t>(v),
                           timescale.numerator_ns,
                           denominator,
                           std::numeric_limits<count_t>::max(),
                           s);

  return checked_narrow<count_t>(static_cast<uint64_t>(v), "timestamp", s);
}

} // namespace detail

inline std::vector<VcdUpdate> parseVcdUpdates(std::istream& in, std::string_view target_name = "outs", uint32_t scale_factor = default_vcd_scale_factor) {
  std::vector<VcdUpdate> updates;

  std::string target_id;
  bool in_definitions = true;
  bool in_timescale = false;
  detail::VcdTimescale timescale{1, 1};
  count_t current_time = 0;

  std::string line;
  while (std::getline(in, line)) {
    const std::string t = detail::trim(line);
    if (t.empty()) continue;

    if (in_definitions) {
      if (in_timescale) {
        bool ended = false;
        const auto payload = detail::strip_timescale_end(t, ended);
        if (!payload.empty())
          timescale = detail::parse_timescale(payload);
        if (ended || t == "$end")
          in_timescale = false;
        continue;
      }

      if (detail::starts_with(t, "$timescale")) {
        bool ended = false;
        const auto payload = detail::strip_timescale_end(detail::trim(std::string_view(t).substr(10)), ended);
        if (!payload.empty())
          timescale = detail::parse_timescale(payload);
        in_timescale = !ended;
      } else if (detail::starts_with(t, "$var")) {
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
      current_time = detail::parse_timestamp(t, scale_factor, timescale);
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

  if (target_id.empty())
    throw std::runtime_error("VCD target not found: " + std::string(target_name));
  if (updates.empty())
    throw std::runtime_error("VCD target has no value updates: " + std::string(target_name));

  updates.push_back(VcdUpdate{0, current_time}); // add end event

  return updates;
}
