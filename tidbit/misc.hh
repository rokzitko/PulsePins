// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Miscelaneous functions, including parsing, I/O, conversions, string manipulation

#pragma once

#include <bitset>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <string>
#include <functional>
#include <iterator>
#include <utility>
#include <tuple>
#include <string>
#include <string_view>
#include <algorithm>
#include <type_traits>
#include <random>
#include <cstdint>
#include <cctype>
#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <cerrno>
#include <system_error>

//#include <charconv>    // C++17
#include "from_chars.hh" // C++11

#include "parser.hh"

using namespace std::chrono_literals;

// Returns true if the data is sequential (steps of n)
template <typename T>
  bool test_sequential(T *data,
                       const size_t len,
                       const T range = 0,
                       const T step = 1,
                       const bool check_first_is_zero = false,
                       const bool verbose = true)
{
  if (verbose) std::cout << "Testing" << std::endl;
  T prev = 0;
  for (size_t i = 0; i < len; i++, data++) {
    auto val = *data;
    if (range) val &= range-1; // Filter bits
    if (i != 0) {
      T expected = prev+step;
      if (range) expected &= range-1;
      if (val != expected) {
        std::cout << "i=" << i << " expected=" << expected << " got=" << val << " "
          << std::bitset<sizeof(T)*8>(val) << std::endl;
        return false;
      }
    } else if (check_first_is_zero) {
      T expected = 0;
      if (val != expected) {
        std::cout << "i=0 expected=0 got=" << val << " "
          << std::bitset<sizeof(T)*8>(val) << std::endl;
        return false;
      }
    }
    prev = val;
  }
  return true;
}

// Control of counters for testing PIO read-out.
class counter_control {
 protected:
   const uint32_t reset_bit = 1;
   const uint32_t start_bit = 2;
   const uint32_t enable_bit = 4;
   const uint32_t stop_bit = 8;
   pio_out_bits &pout;
 public:
   counter_control(pio_out_bits &_pout) : pout(_pout) {}

   void reset() { // reset counter
     pout.set_for(reset_bit, 100ms);
   }

   void go() { // enable counter and read-out
     pout.set(enable_bit);
   }

   void stop() { // stop readout
     pout.set_for(stop_bit, 10ms);
     pout.clear(enable_bit);
   }
};

template <typename T>
  double microseconds_since(T &initial_time) {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - initial_time).count();
  }

void check_ID(const int tidbit, bool verbose = true, std::ostream &s = std::cout)
{
  mm dev_lw(LWHPSFPGA_OFST, LWH2F_RANGE);
  mm dev_h2f(HPSFPGA_OFST, H2F_RANGE);

  assert(SYSID_QSYS_0_ID == tidbit); // check for consistency

  sysid id(dev_lw, SYSID_BASE, SYSID_ID, verbose, s);                      // lw
  sysid id_tidbit(dev_lw, SYSID_QSYS_0_BASE, SYSID_QSYS_0_ID, verbose, s); // lw
  sysid id2(dev_h2f, SYSID_H2F_BASE, SYSID_H2F_ID, verbose, s);            // h2f
}

#include "parseVerilog.hh"

std::string stripUnderscores(const std::string& s) {
  std::string result;
  result.reserve(s.size());
  for (char c : s)
    if (c != '_') result.push_back(c);
  return result;
}

bool containsChar(const std::string& s, char c) {
  return std::find(s.begin(), s.end(), c) != s.end();
}

uint8_t parse_uint8_t(std::string s)
{
  s = stripUnderscores(s);
  if (containsChar(s, '\'')) return parseVerilogInt(s);
  if (s.substr(0, 2) == "0b"s) {
    return strtoul(s.substr(2).c_str(), 0, 2);
  } else {
    std::stringstream ss(s);
    ss >> std::setbase(0);
    uint8_t i;
    ss >> i;
    return i;
  }
}

uint8_t parse_uint8(const InputParser &input, const std::string s, const std::string def) {
  return parse_uint8_t(input.get_string(s, def));
}

uint32_t parse_uint32_t(std::string s)
{
  s = stripUnderscores(s);
  if (containsChar(s, '\'')) return parseVerilogInt(s);
  if (s.substr(0, 2) == "0b"s) {
    return strtoul(s.substr(2).c_str(), 0, 2);
  } else {
    std::stringstream ss(s);
    ss >> std::setbase(0);
    uint32_t i;
    ss >> i;
      return i;
  }
}

uint32_t parse_uint32(const InputParser &input, const std::string s, const std::string def) {
  return parse_uint32_t(input.get_string(s, def));
}

uint64_t parse_uint64_t(std::string s)
{
  s = stripUnderscores(s);
  if (containsChar(s, '\'')) return parseVerilogInt(s);
  if (s.substr(0, 2) == "0b"s) {
    return strtoull(s.substr(2).c_str(), 0, 2);
  } else {
    std::stringstream ss(s);
    ss >> std::setbase(0);
    uint64_t i;
      ss >> i;
    return i;
  }
}

uint64_t parse_uint64(const InputParser &input, const std::string s, const std::string def) {
  return parse_uint64_t(input.get_string(s, def));
}

double parse_double(const InputParser &input, const std::string s, const std::string def) {
  return std::stod(input.get_string(s, def));
}

// Returns a hexadecimal and binary representation of uint32_t 'a'
std::string dump(uint32_t a, std::string sep = "=")
{
   std::stringstream s;
   s << std::dec << a << sep << "0x" << std::hex << a << sep << "0b" << std::bitset<32>(a);
   return s.str();
}

// Returns a hexadecimal and binary representation of uint64_t 'a'
std::string dump(uint64_t a, std::string sep = "=")
{
   std::stringstream s;
   s << std::dec << a << sep << "0x" << std::hex << a << sep << "0b" << std::bitset<64>(a);
   return s.str();
}

// Returns a binary representation of value_t variable
template <typename T> std::string binary_digits(T v)
{
  std::stringstream ss;
  ss << std::bitset<sizeof(T)*8>(v);
  return ss.str();
}

// Returns a hexadecimal (zero padded) and binary representation of 'v'.
template <typename T> std::string hex_and_bin(T v)
{
  std::stringstream ss;
  ss << "0x" << std::hex << std::setw(sizeof(T)*2) << std::setfill('0') << v << " " << std::bitset<sizeof(T)*8>(v);
  return ss.str();
}

inline uint32_t lower32(const uint64_t x) {
  return (uint32_t)x;
}

inline uint32_t higher32(const uint64_t x) {
  return (uint32_t)(x >> 32);
}

template<typename T, typename Cmp, typename Merge>
void merge_adjacent(std::deque<T>& dq, Cmp cmp, Merge merge) {
  if (dq.size() < 2) return;
  for (auto it = dq.begin(); it != dq.end(); /* advance inside */) {
    auto next = std::next(it);
    if (next == dq.end()) break;            // no pair available
    if (cmp(*it, *next)) {
      T new_elem = merge(*it, *next);
      // Remove the pair [it, next] and insert the merged element at 'it'
      it = dq.erase(it, std::next(next)); // returns position of first erased (now where we insert)
      it = dq.insert(it, std::move(new_elem));
      // Do NOT ++it here; we may want to merge the freshly inserted element with the following one.
    } else {
      ++it; // advance only when no merge happened
    }
  }
}

template <typename T>
std::string with_underscores(T value) {
  static_assert(std::is_integral<T>::value, "Integral type required");

  bool negative = value < 0;
  if (negative) value = -value;

  std::string digits = std::to_string(value);
  std::string result;
  int count = 0;

  // Insert underscores from the back
  for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
    if (count && count % 3 == 0) {
      result.push_back('_');
    }
    result.push_back(*it);
    ++count;
  }

  if (negative) result.push_back('-');

  // reverse back to correct order
  std::reverse(result.begin(), result.end());
  return result;
}

// 32-bit integer random number generator using std::random_device and high resolution clock to generate the seed.
class rnd32 {
 public:
   rnd32() {
     // combine random_device and timestamp to reduce chance of collisions
     uint64_t seed = (static_cast<uint64_t>(rd()) << 32)
       ^ static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
     gen.seed(static_cast<uint32_t>(seed));
   }

   uint32_t operator()() {
     return dist(gen);
   }

   uint32_t next() {
     return (*this)();
   }

 private:
   std::random_device rd;
   std::mt19937 gen;
   std::uniform_int_distribution<uint32_t> dist{0, UINT32_MAX};
};

// Simple 32-bit integer random generator
uint32_t random_u32() {
  static std::mt19937 gen(std::random_device{}());
  return gen();  // already uniform over [0, 2^32-1]
}

uint32_t random_log_uniform(uint32_t min_len, uint32_t max_len) {
  if (!(min_len > 0) || !(max_len > min_len))
    throw std::invalid_argument("Require 0 < min_len < max_len.");
  // u in [0,1): divide by 2^32
  constexpr double inv_2p32 = 1.0 / 4294967296.0; // 2^32
  const double u = static_cast<double>(random_u32()) * inv_2p32;
  // x = min_len * (max_len/min_len)^u
  return min_len * std::exp(u * std::log(double(max_len) / double(min_len)));
}

uint32_t random_lin_uniform(uint32_t min_len, uint32_t max_len) {
  return (random_u32() % max_len) + min_len;
}

// Shift Left Logical (SLL)
inline uint32_t sll(uint32_t value, unsigned int shamt) {
  return (shamt < 32) ? (value << shamt) : 0u;
}

// Shift Right Logical (SRL)
inline uint32_t srl(uint32_t value, unsigned int shamt) {
  return (shamt < 32) ? (value >> shamt) : 0u;
}

// Shift Right Arithmetic (SRA)
inline uint32_t sra(uint32_t value, unsigned int shamt) {
  if (shamt >= 32) {
    // Fill entirely with sign bit
    return (value & 0x80000000u) ? 0xFFFFFFFFu : 0u;
  }
  int32_t signed_val = static_cast<int32_t>(value);
  return static_cast<uint32_t>(signed_val >> shamt);
}

bool envVarExists(const std::string &name) {
  return std::getenv(name.c_str()) != nullptr;
}

std::optional<double> envDouble(std::string_view name)
{
  const char* s = std::getenv(name.data());
  if (!s) return std::nullopt;

  errno = 0;
  char* end = nullptr;
  double v = std::strtod(s, &end);

  if (end == s) return std::nullopt;              // no conversion
  if (*end != '\0') return std::nullopt;          // trailing junk
  if (errno == ERANGE) return std::nullopt;       // overflow/undeerflow
  if (!std::isfinite(v)) return std::nullopt;     // optional policy
  return v;
}

std::optional<long long> envInt(std::string_view name, int base = 10)
{
  const char* s = std::getenv(name.data());
  if (!s) return std::nullopt;

  long long v{};
  const char* begin = s;
  const char* end   = s;
  while (*end) ++end; // find NUL

  auto [ptr, ec] = charconv11::from_chars(begin, end, v, base);
  if (ec != charconv11::errc{} || ptr != end) return std::nullopt; // reject trailing junk
  return v;
}

std::optional<bool> envBool(std::string_view name)
{
  const char* s = std::getenv(name.data());
  if (!s) return std::nullopt;

  std::string t{s};
  std::transform(t.begin(), t.end(), t.begin(),
                 [](unsigned char c){ return (char)std::tolower(c); });

  if (t == "1" || t == "true" || t == "yes" || t == "on")  return true;
  if (t == "0" || t == "false"|| t == "no"  || t == "off") return false;
  return std::nullopt;
}

// Helper: trim trailing zeros and decimal point
inline std::string trim_zeros(std::string s) {
  if (s.find('.') != std::string::npos) {
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
  }
  return s;
}

std::string pretty_time(double seconds, int sig_digits = 8) {
  if (seconds == 0.0)
    return "0";

  struct Unit {
    const char* name;
    double factor; // multiply seconds by factor
  };
  static const Unit units[] = {
    {"ns", 1e9},
    {"µs", 1e6},
    {"ms", 1e3},
    {"s",  1.0},
    {"min", 1.0/60.0},
    {"h",   1.0/3600.0},
    {"d",   1.0/86400.0}
  };

  for (const auto& u : units) {
    double val = seconds * u.factor;
    double absval = std::fabs(val);
    if ((absval >= 1.0 && absval < 1000.0) || std::string(u.name) == "d") {
      std::ostringstream oss;
      oss << std::setprecision(sig_digits) << std::scientific << val;

      // Format as fixed if possible, to avoid unnecessary exponents
      std::ostringstream test;
      test << std::setprecision(sig_digits) << std::fixed << val;
      if (test.str().size() < oss.str().size())
        oss.str(test.str());

      return trim_zeros(oss.str()) + u.name;
    }
  }
  return "NaN";
}

std::string pretty_frequency(double hz, int sig_digits = 8) {
  if (hz == 0.0)
    return "0";

  struct Unit {
    const char* name;
    double factor; // multiply Hz by factor
  };
  static const Unit units[] = {
    {"µHz", 1e6},   // 1 Hz = 1e6 µHz
    {"mHz", 1e3},   // 1 Hz = 1e3 mHz
    {"Hz",  1.0},
    {"kHz", 1e-3},
    {"MHz", 1e-6},
    {"GHz", 1e-9}
  };

  for (const auto& u : units) {
    double val = hz * u.factor;
    double absval = std::fabs(val);
    if ((absval >= 1.0 && absval < 1000.0) || std::string(u.name) == "GHz") {
      std::ostringstream oss;
      oss << std::setprecision(sig_digits) << std::fixed << val;
      return trim_zeros(oss.str()) + u.name;
    }
  }
  return "NaN";
}

// Helper: trim spaces from both ends
inline std::string trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  size_t end   = s.find_last_not_of(" \t\n\r");
  return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Helper: split numeric and unit part
inline void split_number_unit(const std::string& input, std::string& number, std::string& unit) {
  std::string s = trim(input);
  size_t pos = 0;

  // scan number part (digits, sign, dot, exp)
  bool in_exp = false;
  while (pos < s.size()) {
    char c = s[pos];
    if (std::isdigit(c) || c == '+' || c == '-' || c == '.') {
      pos++;
    } else if ((c == 'e' || c == 'E') && !in_exp) {
      in_exp = true;
      pos++;
    } else {
      break;
    }
  }
  number = trim(s.substr(0, pos));
  unit   = trim(s.substr(pos));
}

// Parse time strings -> seconds
double parse_time(const std::string& input) {
  if (input == "0")
    return 0.0;

  static const std::unordered_map<std::string, double> factors = {
    {"ns", 1e-9},
    {"us", 1e-6}, // micro
    {"ms", 1e-3},
    {"s",  1.0}, {"sec", 1.0}, {"secs", 1.0},
    {"min", 60.0},
    {"h", 3600.0}, {"hr", 3600.0}, {"hrs", 3600.0},
    {"d", 86400.0}, {"day", 86400.0}, {"days", 86400.0}
  };

  std::string number, unit;
  split_number_unit(input, number, unit);

  if (number.empty())
    throw std::invalid_argument("parse_time: missing number in '" + input + "'");

  char* endptr = nullptr;
  double value = std::strtod(number.c_str(), &endptr);
  if (endptr == number.c_str())
    throw std::invalid_argument("parse_time: invalid number in '" + input + "'");

  if (unit.empty()) return value; // default seconds

  // lowercase unit
  for (auto& c : unit) c = static_cast<char>(std::tolower(c));

  auto it = factors.find(unit);
  if (it == factors.end())
    throw std::invalid_argument("parse_time: unknown unit '" + unit + "'");
  return value * it->second;
}

double parse_time(const InputParser &input, std::string s, std::string def) {
  return parse_time(input.get_string(s, def));
}

// Parse frequency strings -> Hz
double parse_frequency(const std::string& input) {
  if (input == "0")
    return 0.0;

  static const std::unordered_map<std::string, double> factors = {
    {"uhz", 1e-6}, // micro
    {"mhz", 1e-3}, // milli
    {"hz",  1.0},
    {"khz", 1e3},
    {"Mhz", 1e6}, // mega
    {"ghz", 1e9}
  };

  std::string number, unit;
  split_number_unit(input, number, unit);

  if (number.empty())
    throw std::invalid_argument("parse_frequency: missing number in '" + input + "'");

  char* endptr = nullptr;
  double value = std::strtod(number.c_str(), &endptr);
  if (endptr == number.c_str())
    throw std::invalid_argument("parse_frequency: invalid number in '" + input + "'");

  if (unit.empty()) return value; // default Hz

  const bool isM = unit[0] == 'M'; // exception to case insensitivity
  for (auto& c : unit) c = static_cast<char>(std::tolower(c));
  if (isM) unit[0] = 'M';

  auto it = factors.find(unit);
  if (it == factors.end())
    throw std::invalid_argument("parse_frequency: unknown unit '" + unit + "'");
  return value * it->second;
}

double parse_frequency(const InputParser &input, std::string s, std::string def) {
  return parse_frequency(input.get_string(s, def));
}

// assert_not_reached equivalent
void never_reached()
{
  throw std::logic_error("Unreachable code reached");
}

// Bitwise majority of three integers
template <typename T>
constexpr T bitwise_majority(T a, T b, T c) {
  return (a & b) | (a & c) | (b & c);
}

template <typename T>
constexpr T bitwise_majority_nonstrict(T a, T b, T c, T d) {
  return (a & b) | (a & c) | (a & d)
    | (b & c) | (b & d) | (c & d);
}

// Bitwise majority of four inputs: at least 3 out of 4 bits set
template <typename T>
constexpr T bitwise_majority4(T a, T b, T c, T d) {
  return (a & b & c) |
    (a & b & d) |
    (a & c & d) |
    (b & c & d);
}

std::pair<std::string_view, std::string_view> split_once(std::string_view str, char delimiter)
{
  size_t pos = str.find(delimiter);
  if (pos == std::string_view::npos)
    return {str, std::string_view{}};  // no delimiter found
  return {str.substr(0, pos), str.substr(pos + 1)};
}

std::tuple<std::string_view, std::string_view, std::string_view>
  split_twice(std::string_view str, char delimiter)
{
  size_t pos1 = str.find(delimiter);
  if (pos1 == std::string_view::npos)
    return {str, std::string_view{}, std::string_view{}};  // no delimiter
  size_t pos2 = str.find(delimiter, pos1 + 1);
  if (pos2 == std::string_view::npos)
    return {str.substr(0, pos1), str.substr(pos1 + 1), std::string_view{}};  // only one delimiter
  return {
    str.substr(0, pos1),
      str.substr(pos1 + 1, pos2 - pos1 - 1),
      str.substr(pos2 + 1)
  };
}

bool parse_bool(const std::string& s)
{
  // Trim leading/trailing spaces
  auto str = s;
  str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char c){ return !std::isspace(c); }));
  str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char c){ return !std::isspace(c); }).base(), str.end());
  // Convert to lowercase
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c){ return std::tolower(c); });
  if (str == "true" || str == "t" || str == "1" || str == "yes" || str == "y")
    return true;
  if (str == "false" || str == "f" || str == "0" || str == "no" || str == "n")
    return false;
  throw std::invalid_argument("Invalid boolean string: " + s);
}

bool parse_bool(std::string_view s)
{
  return parse_bool(std::string(s));
}

bool parse_bool(const InputParser &input, const std::string s, const std::string def) {
  return parse_bool(input.get_string(s, def));
}

// Example: auto seq_retrig = with_pushed(seq, el(Retrig{}));
template <typename Container, typename T>
  Container with_pushed(Container c, T&& value) {
    c.push_back(std::forward<T>(value));
    return c;
  }

// substring test
bool contains_case(const std::string &text, const std::string &substr) {
  if (substr.empty()) return true;
  return text.find(substr) != std::string::npos;
}

// substring test, case insensitive
bool contains_icase(const std::string &text, const std::string &substr) {
  if (substr.empty()) return true;
  if (text.size() < substr.size()) return false;
  auto it = std::search(text.begin(), text.end(),
                        substr.begin(), substr.end(),
                        [](unsigned char ch1, unsigned char ch2) {
                          return std::tolower(ch1) == std::tolower(ch2);
                        });
  return it != text.end();
}

std::ostream& operator<<(std::ostream& os, uint8_t v) {
    return os << static_cast<unsigned int>(v);
}

std::ostream& operator<<(std::ostream& os, int8_t v) {
    return os << static_cast<int>(v);
}

template <typename T>
  std::string str_hex(const T x) {
    std::stringstream s;
    s << "0x" << std::hex << std::setw(sizeof(T)*2) << std::setfill('0') << x;
        return s.str();
  }

template <typename T>
  std::string str_bin(const T x) {
    std::stringstream s;
    s << "b" << std::bitset<sizeof(T)*8>(x);
        return s.str();
  }

template <typename T>
  std::string str_dec(const T x) {
    std::stringstream s;
    s << std::dec << x;
        return s.str();
  }

template <typename T>
  std::string output_formatter(const T x, const std::string mode)
{
  const auto strhex = contains_icase(mode, "hex") ? str_hex(x) : "";
  const auto strbin = contains_icase(mode, "bin") ? str_bin(x) : "";
  const auto strdec = contains_icase(mode, "dec") ? str_dec(x) : "";
    return trim(strhex + " " + strbin + " " + strdec);
}

std::string timestamp_iso8601_utc_ms() {
  using namespace std::chrono;
  auto now = system_clock::now();
  // Extract whole seconds and milliseconds
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  std::time_t t = system_clock::to_time_t(now);
  std::tm utc_tm = *std::gmtime(&t);
  std::ostringstream oss;
  oss << std::put_time(&utc_tm, "%FT%T")
    << '.' << std::setw(3) << std::setfill('0') << ms.count()
    << 'Z';
  return oss.str();
}

std::string get_env(const std::string& name) {
  const char* val = std::getenv(name.c_str());
  return val ? std::string(val) : std::string();  // empty string if not found
}

inline auto if_nonempty_or(const std::string a, const std::string b)
{
  return a != ""s ? a : b;
}

inline uint64_t to64(uint32_t lo, uint32_t hi)
{
  return uint64_t(lo) + (uint64_t(hi) << 32);
}
