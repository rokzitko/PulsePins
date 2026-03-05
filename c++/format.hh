#pragma once

#include <charconv>
#include <string_view>
#include <system_error>  // std::errc
#include <optional>

std::optional<int> to_int(std::string_view sv) {
  int value{};
  const char* first = sv.data();
  const char* last  = first + sv.size();
  auto [ptr, ec] = std::from_chars(first, last, value, 10);
  // Require full consumption (no trailing junk) and success
  if (ec == std::errc{} && ptr == last) return value;
  return std::nullopt;
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
