#pragma once

#include <string>

#include <functional>
#include <iterator>
#include <utility>
#include <tuple>
#include <string_view>
#include <algorithm>
#include <cstdint>
#include <cctype>
#include <stdexcept>

using namespace std::string_literals;

uint64_t parseVerilogInt(const std::string& lit) {
  std::string s;
  // Remove underscores
  for (char c : lit) {
    if (c != '_') s.push_back(c);
  }

  size_t pos = 0;
  // Check for optional size
  while (pos < s.size() && std::isdigit(s[pos])) pos++;

  int base = 10;
  std::string digits;
  if (pos < s.size() && s[pos] == '\'') {
    // Literal with base
    pos++;
    if (pos >= s.size()) throw std::runtime_error("Invalid Verilog literal: missing base");
    char b = std::tolower(s[pos++]);
    switch (b) {
    case 'b': base = 2; break;
    case 'o': base = 8; break;
    case 'd': base = 10; break;
            case 'h': base = 16; break;
    default: throw std::runtime_error("Invalid Verilog base specifier");
    }
    if (pos >= s.size()) throw std::runtime_error("Invalid Verilog literal: missing digits");
    digits = s.substr(pos);
  } else {
    // No base specifier
    digits = s.substr(pos);
    if (digits.empty()) throw std::runtime_error("Invalid Verilog literal: empty");
    // Old Verilog: leading 0 means octal, otherwise decimal
    if (digits.size() > 1 && digits[0] == '0') base = 8;
  }

  // Convert digits
  uint64_t val = 0;
  for (char c : digits) {
    int v;
    if (std::isdigit(c)) v = c - '0';
    else if (std::isalpha(c)) {
      v = std::tolower(c) - 'a' + 10;
    } else {
      throw std::runtime_error("Invalid digit in Verilog literal");
    }
    if (v >= base) throw std::runtime_error("Digit out of range for base");
    val = val * base + v;
  }
  return val;
}
