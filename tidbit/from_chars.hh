#pragma once
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace charconv11 {

enum class errc { ok = 0, invalid_argument, result_out_of_range };

struct from_chars_result {
  const char* ptr;
  errc ec;
};

struct to_chars_result {
  char* ptr;
  errc ec;
};

namespace detail {

inline int digit_of(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
  return -1;
}

template <class UInt>
inline bool mul_add_overflow(UInt x, UInt mul, UInt add, UInt& out) {
  // out = x*mul + add; detect overflow for unsigned
  if (mul != 0 && x > (std::numeric_limits<UInt>::max)() / mul) return true;
  UInt y = x * mul;
  if (add > (std::numeric_limits<UInt>::max)() - y) return true;
  out = y + add;
  return false;
}

template <class UInt>
inline UInt uabs_of_signed(typename std::make_signed<UInt>::type v) {
  // v is signed; return abs(v) as unsigned, safely for INT_MIN
  typedef typename std::make_signed<UInt>::type S;
  typedef typename std::make_unsigned<S>::type U;
  if (v >= 0) return static_cast<U>(v);
  // Convert using two's complement trick: abs(INT_MIN) is representable in unsigned.
  return static_cast<U>(0) - static_cast<U>(v);
}

} // namespace detail

template <class Int>
from_chars_result from_chars(const char* first, const char* last, Int& value, int base = 10) {
  static_assert(std::is_integral<Int>::value, "from_chars: integral required");
  if (base < 2 || base > 36) return { first, errc::invalid_argument };
  const char* p = first;

  if (p == last) return { first, errc::invalid_argument };

  bool neg = false;
  if (std::is_signed<Int>::value) {
    if (*p == '+') { ++p; }
    else if (*p == '-') { neg = true; ++p; }
  } else {
    if (*p == '+') { ++p; }
    else if (*p == '-') { return { first, errc::invalid_argument }; }
  }

  if (p == last) return { first, errc::invalid_argument };

  // Parse into unsigned accumulator
  typedef typename std::make_unsigned<Int>::type UInt;
  UInt acc = 0;
  bool any = false;

  for (; p != last; ++p) {
    int d = detail::digit_of(*p);
    if (d < 0 || d >= base) break;
    any = true;

    UInt tmp = 0;
    if (detail::mul_add_overflow(acc, static_cast<UInt>(base), static_cast<UInt>(d), tmp)) {
      // consume the rest of valid digits per std::from_chars behavior? It stops at overflow;
      // keeping it strict: return overflow at current ptr.
      return { p, errc::result_out_of_range };
    }
    acc = tmp;
  }

  if (!any) return { first, errc::invalid_argument };

  // Range check and assign to Int
  if (std::is_signed<Int>::value) {
    // allowed magnitude for negative: max_abs_neg = -(min) (e.g. 2147483648 for int32)
    const UInt max_pos = static_cast<UInt>((std::numeric_limits<Int>::max)());
    const UInt max_abs_neg = static_cast<UInt>(0) - static_cast<UInt>((std::numeric_limits<Int>::min)());

    if (neg) {
      if (acc > max_abs_neg) return { p, errc::result_out_of_range };
      // value = -acc (including INT_MIN case)
      value = static_cast<Int>(0) - static_cast<Int>(acc);
    } else {
      if (acc > max_pos) return { p, errc::result_out_of_range };
      value = static_cast<Int>(acc);
    }
  } else {
    value = static_cast<Int>(acc);
  }

  return { p, errc::ok };
}

template <class Int>
to_chars_result to_chars(char* first, char* last, Int value, int base = 10) {
  static_assert(std::is_integral<Int>::value, "to_chars: integral required");
  if (base < 2 || base > 36) return { first, errc::invalid_argument };

  const char* digits = "0123456789abcdefghijklmnopqrstuvwxyz";

  typedef typename std::make_unsigned<Int>::type UInt;
  UInt u = 0;
  bool neg = false;

  if (std::is_signed<Int>::value && value < 0) {
    neg = true;
    u = detail::uabs_of_signed<UInt>(value);
  } else {
    u = static_cast<UInt>(value);
  }

  // Produce digits in reverse
  char buf[std::numeric_limits<UInt>::digits10 + 3]; // enough for base10, plus sign
  std::size_t n = 0;

  do {
    UInt q = u / static_cast<UInt>(base);
    UInt r = u - q * static_cast<UInt>(base);
    buf[n++] = digits[static_cast<unsigned>(r)];
    u = q;
  } while (u != 0);

  if (neg) buf[n++] = '-';

  if (static_cast<std::size_t>(last - first) < n) return { first, errc::result_out_of_range };

  // Reverse copy
  for (std::size_t i = 0; i < n; ++i) {
    first[i] = buf[n - 1 - i];
  }
  return { first + n, errc::ok };
}

} // namespace charconv11
