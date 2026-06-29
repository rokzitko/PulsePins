#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace std::string_literals;

struct ParsedVerilogInteger {
  uint64_t width = 0;
  bool explicit_width = false;
  bool is_signed = false;
  int base = 10;
  bool unbased_unsized = false;
  bool truncated_known_bits = false;
  bool truncated_unknown_bits = false;
  std::vector<uint64_t> value_words;
  std::vector<uint64_t> unknown_words;

  bool has_unknown() const {
    return std::any_of(unknown_words.begin(), unknown_words.end(), [](uint64_t word) { return word != 0; });
  }

  bool has_known_bits_above(const uint64_t bit) const;
  bool has_unknown_bits_above(const uint64_t bit) const;
  uint64_t low_u64() const { return value_words.empty() ? 0 : value_words.front(); }
};

namespace verilog_integer_detail {

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

inline void trim_words(std::vector<uint64_t> &words) {
  while (!words.empty() && words.back() == 0)
    words.pop_back();
}

inline uint64_t word_count_for_width(const uint64_t width) {
  if (width > (std::numeric_limits<uint64_t>::max)() - 63)
    throw std::runtime_error("Verilog literal width is too large for this host");
  return (width + 63) / 64;
}

inline void ensure_word(std::vector<uint64_t> &words, const uint64_t index) {
  if (index > static_cast<uint64_t>((std::numeric_limits<size_t>::max)() - 1))
    throw std::runtime_error("Verilog literal width is too large for this host");
  if (words.size() <= static_cast<size_t>(index))
    words.resize(static_cast<size_t>(index) + 1, 0);
}

inline void or_low_bits(std::vector<uint64_t> &words, const uint64_t bits) {
  if (bits == 0)
    return;
  ensure_word(words, 0);
  words[0] |= bits;
}

inline bool any_bits_at_or_above(const std::vector<uint64_t> &words, const uint64_t bit) {
  const uint64_t word_index = bit / 64;
  if (word_index >= words.size())
    return false;

  const uint64_t bit_index = bit % 64;
  if (bit_index != 0) {
    const uint64_t mask = ~((uint64_t{1} << bit_index) - 1);
    if ((words[static_cast<size_t>(word_index)] & mask) != 0)
      return true;
  } else if (words[static_cast<size_t>(word_index)] != 0) {
    return true;
  }

  for (uint64_t i = word_index + 1; i < words.size(); ++i)
    if (words[static_cast<size_t>(i)] != 0)
      return true;
  return false;
}

inline uint64_t bit_length(const std::vector<uint64_t> &words) {
  for (size_t i = words.size(); i > 0; --i) {
    const uint64_t word = words[i - 1];
    if (word == 0)
      continue;
    uint64_t bits = 64;
    while (bits > 0 && ((word >> (bits - 1)) & 1u) == 0)
      --bits;
    return static_cast<uint64_t>(i - 1) * 64 + bits;
  }
  return 0;
}

inline void shift_left(std::vector<uint64_t> &words, const unsigned bits) {
  if (words.empty() || bits == 0)
    return;

  const unsigned word_shift = bits / 64;
  const unsigned bit_shift = bits % 64;
  std::vector<uint64_t> shifted(words.size() + word_shift + 1, 0);
  for (size_t i = 0; i < words.size(); ++i) {
    shifted[i + word_shift] |= words[i] << bit_shift;
    if (bit_shift != 0)
      shifted[i + word_shift + 1] |= words[i] >> (64 - bit_shift);
  }
  trim_words(shifted);
  words = std::move(shifted);
}

inline void mul_add(std::vector<uint64_t> &words, const uint32_t base, const uint32_t digit) {
  constexpr uint64_t limb_mask = (uint64_t{1} << 32) - 1;
  uint64_t carry = digit;
  for (auto &word : words) {
    const uint64_t lo_product = (word & limb_mask) * base + carry;
    const uint64_t hi_product = (word >> 32) * base + (lo_product >> 32);
    word = ((hi_product & limb_mask) << 32) | (lo_product & limb_mask);
    carry = hi_product >> 32;
  }
  while (carry != 0) {
    words.push_back(carry & limb_mask);
    carry >>= 32;
  }
  trim_words(words);
}

inline uint64_t parse_decimal_u64(const std::string &digits, const char *what) {
  if (digits.empty())
    throw std::runtime_error(std::string("Invalid Verilog literal: missing ") + what);
  uint64_t value = 0;
  for (char c : digits) {
    if (!std::isdigit(static_cast<unsigned char>(c)))
      throw std::runtime_error(std::string("Invalid Verilog literal: invalid ") + what);
    const uint64_t digit = static_cast<uint64_t>(c - '0');
    if (value > ((std::numeric_limits<uint64_t>::max)() - digit) / 10)
      throw std::runtime_error(std::string("Invalid Verilog literal: ") + what + " exceeds uint64_t range");
    value = value * 10 + digit;
  }
  return value;
}

inline int digit_value(const char c) {
  if (std::isdigit(static_cast<unsigned char>(c)))
    return c - '0';
  if (std::isalpha(static_cast<unsigned char>(c)))
    return std::tolower(static_cast<unsigned char>(c)) - 'a' + 10;
  return -1;
}

inline bool is_unknown_digit(const char c) {
  const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return lower == 'x' || lower == 'z' || c == '?';
}

inline bool mask_to_width(std::vector<uint64_t> &words, const uint64_t width) {
  const uint64_t keep_words_u64 = word_count_for_width(width);
  if (keep_words_u64 > static_cast<uint64_t>((std::numeric_limits<size_t>::max)()))
    throw std::runtime_error("Verilog literal width is too large for this host");
  const size_t keep_words = static_cast<size_t>(keep_words_u64);
  bool truncated = any_bits_at_or_above(words, width);

  if (words.size() > keep_words)
    words.resize(keep_words);
  if (width % 64 != 0 && !words.empty()) {
    const uint64_t keep_mask = (uint64_t{1} << (width % 64)) - 1;
    words.back() &= keep_mask;
  }
  trim_words(words);
  return truncated;
}

inline uint64_t parsed_width(const ParsedVerilogInteger &parsed) {
  const uint64_t known_width = bit_length(parsed.value_words);
  const uint64_t unknown_width = bit_length(parsed.unknown_words);
  return (std::max)(uint64_t{1}, (std::max)(known_width, unknown_width));
}

inline void apply_width(ParsedVerilogInteger &parsed, const uint64_t parsed_digit_width) {
  if (parsed.explicit_width) {
    parsed.truncated_known_bits = mask_to_width(parsed.value_words, parsed.width);
    parsed.truncated_unknown_bits = mask_to_width(parsed.unknown_words, parsed.width);
    return;
  }

  const uint64_t natural_width = (std::max)(parsed_digit_width, parsed_width(parsed));
  // SystemVerilog unsized integer literals are at least 32 bits wide. `unbased_unsized`
  // literals are context-sized in the language, so keep their one-bit fill representation.
  parsed.width = parsed.unbased_unsized ? 1 : (std::max)(uint64_t{32}, natural_width);
}

inline ParsedVerilogInteger parse_plain_decimal(const std::string &digits) {
  if (digits.empty())
    throw std::runtime_error("Invalid Verilog literal: empty");

  ParsedVerilogInteger parsed;
  parsed.base = 10;
  for (char c : digits) {
    if (!std::isdigit(static_cast<unsigned char>(c)))
      throw std::runtime_error("Invalid digit in Verilog decimal literal");
    mul_add(parsed.value_words, 10, static_cast<uint32_t>(c - '0'));
  }
  apply_width(parsed, parsed_width(parsed));
  return parsed;
}

inline ParsedVerilogInteger parse_unbased_unsized(const char fill) {
  ParsedVerilogInteger parsed;
  parsed.unbased_unsized = true;
  parsed.width = 1;
  switch (static_cast<char>(std::tolower(static_cast<unsigned char>(fill)))) {
  case '0':
    break;
  case '1':
    parsed.value_words.push_back(1);
    break;
  case 'x':
  case 'z':
  case '?':
    parsed.unknown_words.push_back(1);
    break;
  default:
    throw std::runtime_error("Invalid Verilog unbased unsized literal");
  }
  return parsed;
}

inline ParsedVerilogInteger parse_based_digits(const std::string &digits, const int base) {
  if (digits.empty())
    throw std::runtime_error("Invalid Verilog literal: missing digits");

  ParsedVerilogInteger parsed;
  parsed.base = base;

  if (base == 10) {
    for (char c : digits) {
      if (!std::isdigit(static_cast<unsigned char>(c)))
        throw std::runtime_error("Invalid digit in Verilog decimal literal");
      mul_add(parsed.value_words, 10, static_cast<uint32_t>(c - '0'));
    }
    apply_width(parsed, parsed_width(parsed));
    return parsed;
  }

  const unsigned bits_per_digit = base == 2 ? 1 : (base == 8 ? 3 : 4);
  for (char c : digits) {
    shift_left(parsed.value_words, bits_per_digit);
    shift_left(parsed.unknown_words, bits_per_digit);
    if (is_unknown_digit(c)) {
      or_low_bits(parsed.unknown_words, (uint64_t{1} << bits_per_digit) - 1);
      continue;
    }
    const int value = digit_value(c);
    if (value < 0 || value >= base)
      throw std::runtime_error("Digit out of range for Verilog literal base");
    or_low_bits(parsed.value_words, static_cast<uint64_t>(value));
  }
  apply_width(parsed, static_cast<uint64_t>(digits.size()) * bits_per_digit);
  return parsed;
}

} // namespace verilog_integer_detail

inline bool ParsedVerilogInteger::has_known_bits_above(const uint64_t bit) const {
  return verilog_integer_detail::any_bits_at_or_above(value_words, bit);
}

inline bool ParsedVerilogInteger::has_unknown_bits_above(const uint64_t bit) const {
  return verilog_integer_detail::any_bits_at_or_above(unknown_words, bit);
}

inline ParsedVerilogInteger parseSystemVerilogInteger(const std::string &lit) {
  using namespace verilog_integer_detail;

  const std::string s = strip_underscores(trim(lit));
  if (s.empty())
    throw std::runtime_error("Invalid Verilog literal: empty");
  if (s.front() == '+' || s.front() == '-')
    throw std::runtime_error("Invalid Verilog literal: sign must be supplied by the expression, not the literal");

  const size_t quote = s.find('\'');
  if (quote == std::string::npos)
    return parse_plain_decimal(s);
  if (s.find('\'', quote + 1) != std::string::npos)
    throw std::runtime_error("Invalid Verilog literal: multiple apostrophes");

  ParsedVerilogInteger parsed;
  if (quote != 0) {
    parsed.explicit_width = true;
    parsed.width = parse_decimal_u64(s.substr(0, quote), "width");
    if (parsed.width == 0)
      throw std::runtime_error("Invalid Verilog literal: width must be greater than zero");
  }

  size_t pos = quote + 1;
  if (pos >= s.size())
    throw std::runtime_error("Invalid Verilog literal: missing base");

  if (s[pos] == 's' || s[pos] == 'S') {
    parsed.is_signed = true;
    pos++;
    if (pos >= s.size())
      throw std::runtime_error("Invalid Verilog literal: missing base after signed marker");
  }

  const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(s[pos])));
  int base = 0;
  switch (b) {
  case 'b': base = 2; break;
  case 'o': base = 8; break;
  case 'd': base = 10; break;
  case 'h': base = 16; break;
  default:
    if (!parsed.explicit_width && !parsed.is_signed && pos + 1 == s.size())
      return parse_unbased_unsized(s[pos]);
    throw std::runtime_error("Invalid Verilog base specifier");
  }
  pos++;

  ParsedVerilogInteger digits = parse_based_digits(s.substr(pos), base);
  digits.explicit_width = parsed.explicit_width;
  digits.width = parsed.width;
  digits.is_signed = parsed.is_signed;
  digits.base = base;
  apply_width(digits, digits.width);
  return digits;
}

inline uint64_t parseVerilogInt(const std::string& lit) {
  const auto parsed = parseSystemVerilogInteger(lit);
  if (parsed.has_unknown())
    throw std::runtime_error("Verilog literal contains unknown/high-impedance bits");
  if (parsed.has_known_bits_above(64))
    throw std::runtime_error("Verilog literal exceeds uint64_t range");
  return parsed.low_u64();
}
