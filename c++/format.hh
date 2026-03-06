#pragma once

#include <string_view>
#include <system_error>  // std::errc
#include <optional>
#include <cstdlib>
#include <cerrno>
#include <climits>

std::optional<int> to_int(std::string_view sv) {
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

std::string setw_l(std::string s, std::string_view w) {
  std::stringstream ss;
  ss << std::setw(to_int(w).value_or(0)) << std::left << s;
  return ss.str();
}

// uint32_t as 0x00112233, zero padded
std::string hex8(uint32_t x) {
  std::stringstream ss;
  ss << "0x" << std::setw(8) << std::setfill('0') << std::hex << x;
  return ss.str();
}

// uint32_t as 1_234_567_890, right aligned
std::string dec13(uint32_t x) {
  std::stringstream ss;
  ss << std::setw(13) << std::setfill(' ') << with_underscores(x);
  return ss.str();
}
