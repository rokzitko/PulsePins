// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include "misc.hh"

inline int64_t parse_nonnegative_int64(const InputParser &input, const std::string &option, const std::string &def) {
  const auto value = parse_uint64(input, option, def);
  if (value > static_cast<uint64_t>((std::numeric_limits<int64_t>::max)()))
    throw std::runtime_error(option + " must be in range 0.." + std::to_string((std::numeric_limits<int64_t>::max)()));
  return static_cast<int64_t>(value);
}

inline int64_t signed_counter_delta(const uint64_t later, const uint64_t earlier) {
  const uint64_t delta = later - earlier;
  if (delta <= static_cast<uint64_t>((std::numeric_limits<int64_t>::max)()))
    return static_cast<int64_t>(delta);

  const uint64_t magnitude = ~delta + 1;
  if (magnitude == (uint64_t{1} << 63))
    return (std::numeric_limits<int64_t>::min)();
  return -static_cast<int64_t>(magnitude);
}

inline int64_t checked_i64_sub(const int64_t a, const int64_t b, const char *context) {
  if (b > 0 && a < (std::numeric_limits<int64_t>::min)() + b)
    throw std::overflow_error(context);
  if (b < 0 && a > (std::numeric_limits<int64_t>::max)() + b)
    throw std::overflow_error(context);
  return a - b;
}

inline uint64_t abs_i64_to_u64(const int64_t value) {
  if (value >= 0)
    return static_cast<uint64_t>(value);
  return static_cast<uint64_t>(-(value + 1)) + 1;
}
