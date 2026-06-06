// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Simple command-line parameter parser

#pragma once

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace strict_numeric_detail {

inline std::string trim(std::string_view input) {
  while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front())))
    input.remove_prefix(1);
  while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back())))
    input.remove_suffix(1);
  return std::string(input);
}

inline std::string strip_underscores(std::string s) {
  s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
  return s;
}

} // namespace strict_numeric_detail

inline double parse_strict_finite_double(std::string_view input, const std::string &context = "number") {
  const std::string s = strict_numeric_detail::trim(input);
  if (s.empty())
    throw std::runtime_error("Invalid " + context + ": empty value");

  errno = 0;
  char *end = nullptr;
  const double value = std::strtod(s.c_str(), &end);
  if (end == s.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(value))
    throw std::runtime_error("Invalid " + context + ": " + s);
  return value;
}

template <typename T>
inline T parse_strict_unsigned_integral(std::string_view input, const std::string &context = "unsigned integer") {
  static_assert(std::is_unsigned<T>::value, "Unsigned integral type required");

  std::string s = strict_numeric_detail::strip_underscores(strict_numeric_detail::trim(input));
  if (s.empty())
    throw std::runtime_error("Invalid " + context + ": empty value");
  if (s.front() == '-')
    throw std::runtime_error("Invalid " + context + ": negative value");
  if (s.front() == '+')
    s.erase(s.begin());
  if (s.empty())
    throw std::runtime_error("Invalid " + context + ": missing digits");

  const bool is_binary_prefix = s.size() > 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B');
  const std::string digits = is_binary_prefix ? s.substr(2) : s;
  if (digits.empty())
    throw std::runtime_error("Invalid " + context + ": missing digits");

  errno = 0;
  char *end = nullptr;
  const unsigned long long value = std::strtoull(digits.c_str(), &end, is_binary_prefix ? 2 : 0);
  if (end == digits.c_str() || *end != '\0' || errno == ERANGE)
    throw std::runtime_error("Invalid " + context + ": " + s);
  if (value > static_cast<unsigned long long>(std::numeric_limits<T>::max()))
    throw std::runtime_error("Invalid " + context + ": out of range");

  return static_cast<T>(value);
}

inline uint32_t parse_strict_uint32(std::string_view input, const std::string &context = "uint32") {
  return parse_strict_unsigned_integral<uint32_t>(input, context);
}

inline uint64_t parse_strict_uint64(std::string_view input, const std::string &context = "uint64") {
  return parse_strict_unsigned_integral<uint64_t>(input, context);
}

// Simple command line parser
// based on https://stackoverflow.com/questions/865668/parsing-command-line-arguments-in-c
class InputParser {
 public:
   InputParser(int argc, char *argv[]) {
     for (int i = 1; i < argc; ++i)
       this->tokens.push_back(std::string(argv[i]));
   }
   InputParser(std::vector<std::string> t) : tokens(t) {}
   std::string get(const std::string &option) const {
     auto itr = std::find(this->tokens.begin(), this->tokens.end(), option);
     if (itr != this->tokens.end() && ++itr != this->tokens.end())
       return *itr;
     return "";
   }
   bool exists(const std::string &option) const {
     return std::find(this->tokens.begin(), this->tokens.end(), option) != this->tokens.end();
   }
   std::string get_string(const std::string &option, const std::string def) const {
     return exists(option) ? require_arg(option) : def;
   }
   double get_double(const std::string &option, const double def) const {
     return exists(option) ? parse_strict_finite_double(require_arg(option), option) : def;
   }
   uint32_t get_uint32(const std::string &option, const uint32_t def) const {
     return exists(option) ? parse_strict_uint32(require_arg(option), option) : def;
   }
   uint64_t get_uint64(const std::string &option, const uint64_t def) const {
     return exists(option) ? parse_strict_uint64(require_arg(option), option) : def;
   }
   void add(const std::string s) {
     tokens.push_back(s);
   }
   void add_with_arg(const std::string s1, const std::string s2) {
     tokens.push_back(s1);
     tokens.push_back(s2);
   }
   std::optional<std::string> first_arg() const {
     if (tokens.size() > 0)
       return tokens.front();
     else
       return std::nullopt;
   }
   std::optional<int> first_arg_int() const {
     if (tokens.size() > 0) {
       auto s = tokens.front();
       if (s.empty())
         return std::nullopt;
       const auto c = static_cast<unsigned char>(s.front());
       if (std::isdigit(c)) {
         try {
           const auto value = parse_strict_uint64(s, "first argument");
           if (value <= static_cast<uint64_t>(std::numeric_limits<int>::max()))
             return static_cast<int>(value);
         } catch (const std::exception &) {
         }
       }
     }
     return std::nullopt;
   }
 private:
   std::string require_arg(const std::string &option) const {
     auto itr = std::find(this->tokens.begin(), this->tokens.end(), option);
     if (itr == this->tokens.end())
       throw std::runtime_error("Missing option: " + option);
     ++itr;
     if (itr == this->tokens.end())
       throw std::runtime_error("Option " + option + " requires an argument");
     return *itr;
   }

   std::vector<std::string> tokens;
};
