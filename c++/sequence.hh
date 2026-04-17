// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Host-side representation of PulsePins sequences.
//
// A Sequence is an ordered container of `el` objects matching the encoded stream sent
// to the FPGA. Most user-visible pulse programs eventually pass through this type,
// either because they were built programmatically or because they were parsed from a
// text/VCD representation. Higher-level architectural context lives in `c++/README.md`
// and `docs/docs/cpp.md`.

#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "elements.hh"
#include "ppversion.hh"
#include "vcd_parser.hh"

inline std::string value_to_vcd_binary(value_t v)
{
  return std::bitset<WIDTH_DATA>(static_cast<unsigned long long>(v)).to_string();
}

struct BinarySequenceHeader {
  std::array<char, 4> magic;
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t endianness;
  uint8_t payload_kind;
  uint16_t width_control;
  uint16_t width_counter;
  uint16_t width_value;
  uint16_t width_trigger;
  uint32_t flags;
  uint32_t header_size;
  uint64_t element_count;
  uint64_t payload_size_bytes;
  uint32_t reserved0;
  uint32_t reserved1;
};

inline constexpr std::array<char, 4> PPBF_MAGIC{{'P', 'P', 'B', 'F'}};
inline constexpr uint8_t PPBF_VERSION_MAJOR = 1;
inline constexpr uint8_t PPBF_VERSION_MINOR = 0;
inline constexpr uint8_t PPBF_ENDIAN_LITTLE = 1;
inline constexpr uint8_t PPBF_PAYLOAD_RAW_TRIPLES = 0;
inline constexpr uint32_t PPBF_FLAG_FORCE_TRIGGER = 1u << 0;
inline constexpr uint32_t PPBF_HEADER_SIZE = 48;

template<typename T>
inline void write_le(std::ostream &f, T value)
{
  for (size_t i = 0; i < sizeof(T); ++i)
    f.put(static_cast<char>((static_cast<uint64_t>(value) >> (8 * i)) & 0xFF));
  if (!f)
    throw std::runtime_error("Failed to write binary sequence data");
}

template<typename T>
inline T read_le(std::istream &f)
{
  uint64_t value = 0;
  for (size_t i = 0; i < sizeof(T); ++i) {
    const int ch = f.get();
    if (ch == std::char_traits<char>::eof())
      throw std::runtime_error("Unexpected end of binary sequence data");
    value |= uint64_t(static_cast<uint8_t>(ch)) << (8 * i);
  }
  return static_cast<T>(value);
}

inline void write_binary_header(std::ostream &f,
                                const uint64_t element_count,
                                const uint64_t payload_size_bytes,
                                const bool force_trigger)
{
  for (const auto c : PPBF_MAGIC)
    f.put(c);
  write_le<uint8_t>(f, PPBF_VERSION_MAJOR);
  write_le<uint8_t>(f, PPBF_VERSION_MINOR);
  write_le<uint8_t>(f, PPBF_ENDIAN_LITTLE);
  write_le<uint8_t>(f, PPBF_PAYLOAD_RAW_TRIPLES);
  write_le<uint16_t>(f, WIDTH_CONTROL);
  write_le<uint16_t>(f, WIDTH_COUNTER);
  write_le<uint16_t>(f, WIDTH_DATA);
  write_le<uint16_t>(f, WIDTH_TRIGGER);
  write_le<uint32_t>(f, force_trigger ? PPBF_FLAG_FORCE_TRIGGER : 0u);
  write_le<uint32_t>(f, PPBF_HEADER_SIZE);
  write_le<uint64_t>(f, element_count);
  write_le<uint64_t>(f, payload_size_bytes);
  write_le<uint32_t>(f, 0u);
  write_le<uint32_t>(f, 0u);
}

inline BinarySequenceHeader read_binary_header(std::istream &f)
{
  BinarySequenceHeader h{};
  for (auto &c : h.magic) {
    const int ch = f.get();
    if (ch == std::char_traits<char>::eof())
      throw std::runtime_error("Unexpected end of binary sequence header");
    c = static_cast<char>(ch);
  }
  h.version_major = read_le<uint8_t>(f);
  h.version_minor = read_le<uint8_t>(f);
  h.endianness = read_le<uint8_t>(f);
  h.payload_kind = read_le<uint8_t>(f);
  h.width_control = read_le<uint16_t>(f);
  h.width_counter = read_le<uint16_t>(f);
  h.width_value = read_le<uint16_t>(f);
  h.width_trigger = read_le<uint16_t>(f);
  h.flags = read_le<uint32_t>(f);
  h.header_size = read_le<uint32_t>(f);
  h.element_count = read_le<uint64_t>(f);
  h.payload_size_bytes = read_le<uint64_t>(f);
  h.reserved0 = read_le<uint32_t>(f);
  h.reserved1 = read_le<uint32_t>(f);
  return h;
}

inline void validate_binary_header(const BinarySequenceHeader &h)
{
  if (h.magic != PPBF_MAGIC)
    throw std::runtime_error("Invalid binary sequence magic");
  if (h.version_major != PPBF_VERSION_MAJOR)
    throw std::runtime_error("Unsupported binary sequence major version");
  if (h.endianness != PPBF_ENDIAN_LITTLE)
    throw std::runtime_error("Unsupported binary sequence endianness");
  if (h.payload_kind != PPBF_PAYLOAD_RAW_TRIPLES)
    throw std::runtime_error("Unsupported binary sequence payload kind");
  if (h.width_control != WIDTH_CONTROL || h.width_counter != WIDTH_COUNTER || h.width_value != WIDTH_DATA || h.width_trigger != WIDTH_TRIGGER)
    throw std::runtime_error("Binary sequence width mismatch");
  if (h.header_size != PPBF_HEADER_SIZE)
    throw std::runtime_error("Unsupported binary sequence header size");
  if (h.payload_size_bytes != h.element_count * (sizeof(control_t) + sizeof(count_t) + sizeof(value_t)))
    throw std::runtime_error("Binary sequence payload size mismatch");
}

inline el regular_element_from_control(control_t control, count_t count, value_t value)
{
  const auto make_element = [control, count](const Value &regular_value) {
    el e = ((control & NOSTROBE) == NOSTROBE) ? el(NoStrobe(count), regular_value) : el(Counter(count), regular_value);
    e.set_control(control);
    return e;
  };

  switch (control & MODEBITS) {
    case BITLOAD: return make_element(BitLoad(value));
    case BITSET: return make_element(BitSet(value));
    case BITCLEAR: return make_element(BitClear(value));
    case BITFLIP: return make_element(BitFlip(value));
    case BITNOT: return make_element(BitNot(value));
    case BITAND: return make_element(BitAnd(value));
    case BITOR: return make_element(BitOr(value));
    case BITXOR: return make_element(BitXor(value));
    case BITXNOR: return make_element(BitXnor(value));
    case BITSLL: return make_element(BitSll(value));
    case BITSRL: return make_element(BitSrl(value));
    default:
      throw std::runtime_error("Unsupported regular element mode in binary sequence reader");
  }
}

inline el element_from_raw_triplet(control_t control, count_t count, value_t value)
{
  if ((control & TRIGGERBITS) == TRIGGER || (control & TRIGGERBITS) == (TRIGGER | TRIGGERFINAL)) {
    const auto pattern = static_cast<trigger_t>(value & TRIGGER_MASK);
    const auto mask = static_cast<trigger_t>((value >> WIDTH_TRIGGER) & TRIGGER_MASK);
    const bool final = (control & TRIGGERFINAL) == TRIGGERFINAL;
    el e(pattern, mask, final);
    e.set_control(control);
    return e;
  }
  if ((control & REPLAY) == REPLAY) {
    el e(Replay{}, count, value);
    e.set_control(control);
    return e;
  }
  if ((control & RETRIG) == RETRIG) {
    el e(Retrig{}, value);
    e.set_control(control);
    return e;
  }
  if ((control & PRNG) == PRNG) {
    el e(PseudoRandom{}, count);
    e.set_control(control);
    return e;
  }
  if ((control & TERMINATE) == TERMINATE) {
    el e(value);
    e.set_control(control);
    return e;
  }

  return regular_element_from_control(control, count, value);
}

// Thin extension of `std::deque<el>` with helpers that reflect the semantics of a
// pulse sequence rather than just container operations.
class Sequence : public std::deque<el> {
public:
  using Base = std::deque<el>;
  using Base::Base;
  using Base::back;
  using Base::clear;
  using Base::empty;
  using Base::insert;
  using Base::push_back;

  void push_back_py(el && x) { Base::push_back(x); }

  // Total length of the sequence (in units of periods)
  uint64_t length() const {
    uint64_t len = 0;
    for (const auto &e : *this)
      if (e.is_regular())
        len += e.count();
    return len;
  }

  // Number of regular elements in the container
  uint64_t data_size() const {
    uint64_t sz = 0;
    for (const auto &e : *this)
      if (e.is_regular())
        sz++;
    return sz;
  }

  // Dump the sequence to stream `F`.
  void dump(std::ostream &F = std::cout, const std::string prefix = "") const {
    F << prefix << "Sequence: number of elements (size)=" << size() << ", sequence duration in clock periods (length)=" << length() << std::endl;
    size_t i = 0;
    for (const auto &e : *this) {
      F << prefix << std::dec << i << ": " << e << std::endl;
      i++;
    }
  }

  void dump_py(const std::string prefix = "") const {
    dump(std::cout, prefix);
  }

    // Convert regular data elements into the effective output-value stream. This is
    // mainly used for readback checking, where comparisons are done against the data
    // observed at the streamer output rather than the original update operators.
    Sequence convert_to_BitLoad() const {
    Sequence s;
    size_t n = 0; // counts regular elements only
    value_t v_prev;
    for (const auto &e: *this) {
      el enew = e;
      if (n && e.is_regular()) {
        enew.set_value(BitLoad(e.updated_value(v_prev)));
        enew.set_control((e.control() & ~MODEBITS) | BITLOAD);
      }
      s.push_back(enew);
      if (e.is_regular())
        v_prev = enew.value();
      if (e.is_regular())
        n++;
    }
    return s;
  }

    // Merge adjacent regular elements that produce the same output state.
    Sequence merge() const {
    Sequence s = *this; // make a copy
    merge_adjacent<el>(s,
                        [](const el &x, const el &y){ return x.is_regular() && y.is_regular() && x.control() == y.control() && x.value() == y.value(); },
                        [](const el &x, const el &y){ el merged = x; merged.set_count(x.count() + y.count()); return merged; });
    return s;
  }

    // Build a sequence from a VCD signal trace. Consecutive samples become run-length
    // encoded elements targeting `target_name`.
    void load_VCD(const std::string filename, const std::string target_name = "outs", const uint32_t scale_factor = 10) {
    std::ifstream F(filename);
    auto l = parseVcdUpdates(F, target_name, scale_factor);
    for (size_t i = 0; i < l.size()-1; i++) {
      Counter c = l[i+1].count-l[i].count;
      this->push_back(el(c, l[i].value));
    }
  }

  void write_VCD(std::ostream &f,
                const std::string &target_name = "outs",
                const std::string &timescale = "1ns") const {
    Sequence s;
    value_t current_value = 0;
    for (const auto &e : *this) {
      if (!e.is_regular()) {
        s.push_back(e);
        continue;
      }
      current_value = e.updated_value(current_value);
      s.push_back(el(e.count(), current_value));
    }
    s.erase(std::remove_if(s.begin(), s.end(), [](const el &e) {
      return e.is_regular() && e.count() == 0;
    }), s.end());
    s = s.merge();

    for (const auto &e : s) {
      if (!e.is_regular())
        throw std::runtime_error("VCD export requires a deterministic regular sequence");
    }

    f << "$date\n";
    f << "  " << __DATE__ << " " << __TIME__ << "\n";
    f << "$end\n";
    f << "$version\n";
    f << "  PulsePins " << VERSION << "\n";
    f << "$end\n";
    f << "$timescale " << timescale << " $end\n";
    f << "$scope module pulsepins $end\n";
    f << "$var reg " << WIDTH_DATA << " ! " << target_name << " [" << (WIDTH_DATA - 1) << ":0] $end\n";
    f << "$upscope $end\n";
    f << "$enddefinitions $end\n";

    if (s.empty()) {
      f << "#0\n";
      f << "b" << value_to_vcd_binary(0) << " !\n";
      return;
    }

    count_t time = 0;
    value_t last_value = s.front().value();
    f << "#0\n";
    f << "b" << value_to_vcd_binary(last_value) << " !\n";

    for (size_t i = 0; i < s.size(); ++i) {
      time += s[i].count();
      if (i + 1 >= s.size())
        break;
      const value_t next_value = s[i + 1].value();
      if (next_value == last_value)
        continue;
      f << "#" << time << "\n";
      f << "b" << value_to_vcd_binary(next_value) << " !\n";
      last_value = next_value;
    }

    f << "#" << time << "\n";
  }

  void write_VCD_file(const std::string &filename,
                      const std::string &target_name = "outs",
                      const std::string &timescale = "1ns") const {
    std::ofstream f(filename);
    if (!f)
      throw std::runtime_error("Could not open VCD output file: " + filename);
    write_VCD(f, target_name, timescale);
  }

  void write_binary(std::ostream &f, const bool force_trigger = false) const {
    const uint64_t element_count = size();
    const uint64_t payload_size_bytes = element_count * (sizeof(control_t) + sizeof(count_t) + sizeof(value_t));
    write_binary_header(f, element_count, payload_size_bytes, force_trigger);
    for (const auto &e : *this) {
      write_le<control_t>(f, e.control());
      write_le<count_t>(f, e.count());
      write_le<value_t>(f, e.value());
    }
  }

  void write_binary_file(const std::string &filename, const bool force_trigger = false) const {
    std::ofstream f(filename, std::ios::binary);
    if (!f)
      throw std::runtime_error("Could not open binary sequence output file: " + filename);
    write_binary(f, force_trigger);
  }

  static std::pair<Sequence, bool> read_binary(std::istream &f) {
    const auto header = read_binary_header(f);
    validate_binary_header(header);
    Sequence seq;
    for (uint64_t i = 0; i < header.element_count; ++i) {
      const auto control = read_le<control_t>(f);
      const auto count = read_le<count_t>(f);
      const auto value = read_le<value_t>(f);
      seq.push_back(element_from_raw_triplet(control, count, value));
    }
    if (f.peek() != std::char_traits<char>::eof())
      throw std::runtime_error("Trailing data after binary sequence payload");
    const bool force_trigger = (header.flags & PPBF_FLAG_FORCE_TRIGGER) != 0;
    return {seq, force_trigger};
  }

  static std::pair<Sequence, bool> read_binary_file(const std::string &filename) {
    std::ifstream f(filename, std::ios::binary);
    if (!f)
      throw std::runtime_error("Could not open binary sequence input file: " + filename);
    return read_binary(f);
  }

  virtual ~Sequence() = default;
};

inline bool compare(const Sequence &X, const Sequence &Y, bool verbose = false) {
  if (X.size() != Y.size()) return false;
  size_t size = X.size();
  for (size_t i = 0; i < size; i++) {
    if (verbose) std::cout << X[i].desc() << "  <->  " << Y[i].desc() << std::endl;
    if (X[i] != Y[i]) return false;
  }
  return true;
}

inline bool operator==(const Sequence &X, const Sequence &Y) {
  return compare(X, Y);
}

inline std::string sequence_regular_token(const el &e)
{
  if (!e.is_regular())
    throw std::runtime_error("sequence_regular_token() requires a regular element");

  const control_t control = e.control();
  const bool no_strobe = (control & NOSTROBE) == NOSTROBE;
  const control_t mode = control & MODEBITS;

  if (no_strobe && mode != BITLOAD)
    throw std::runtime_error("Text sequence writer does not support non-BITLOAD no-strobe elements");

  if (mode == BITLOAD)
    return no_strobe ? "dn" : "d";
  if (mode == BITSET)
    return "s";
  if (mode == BITCLEAR)
    return "c";
  if (mode == BITFLIP)
    return "x";
  if (mode == BITNOT)
    return "n";
  if (mode == BITAND)
    return "a";
  if (mode == BITOR)
    return "o";
  if (mode == BITXOR)
    return "xr";
  if (mode == BITXNOR)
    return "xn";
  if (mode == BITSLL)
    return "sl";
  if (mode == BITSRL)
    return "sr";

  throw std::runtime_error("Unsupported regular element mode in text sequence writer");
}

inline void write_sequence_to_stream(const Sequence &sequence,
                                    std::ostream &f,
                                    const bool force_trigger = false)
{
  for (const auto &e : sequence) {
    const control_t control = e.control();

    if (e.is_regular()) {
      const auto token = sequence_regular_token(e);
      if ((control & STORE) == STORE) {
        const auto slot = (control & POSITIONS_MASK) >> SHIFT_POSITION;
        f << "store " << std::dec << slot << " " << token
          << " " << std::dec << e.count()
          << " 0x" << std::hex << e.value() << "\n";
      } else {
        f << token
          << " " << std::dec << e.count()
          << " 0x" << std::hex << e.value() << "\n";
      }
    } else if (e.is_trigger()) {
      const auto pattern = static_cast<trigger_t>(e.value() & TRIGGER_MASK);
      const auto mask = static_cast<trigger_t>((e.value() >> WIDTH_TRIGGER) & TRIGGER_MASK);
      const bool final = (control & TRIGGERFINAL) == TRIGGERFINAL;
      f << (final ? "t" : "tn")
        << " 0x" << std::hex << int(pattern)
        << " 0x" << std::hex << int(mask) << "\n";
    } else if (e.is_final()) {
      f << "final 0x" << std::hex << e.value() << "\n";
    } else if (e.is_replay()) {
      f << "r"
        << " " << std::dec << e.count()
        << " 0x" << std::hex << e.value() << "\n";
    } else if (e.is_retrig()) {
      f << "rt\n";
    } else if (e.is_prng()) {
      f << "pr"
        << " " << std::dec << e.count() << "\n";
    } else {
      throw std::runtime_error("Unsupported element in text sequence writer");
    }
  }

  if (force_trigger)
    f << "f\n";
}

inline void write_sequence_to_file(const Sequence &sequence,
                                  const std::string &filename,
                                  const bool force_trigger = false)
{
  std::ofstream f(filename);
  if (!f)
    throw std::runtime_error("Could not open sequence output file: " + filename);
  write_sequence_to_stream(sequence, f, force_trigger);
}

inline std::pair<Sequence, bool> parse_sequence_from_stream(std::istream &f)
{
  // Text grammar accepted here:
  //   d <count> <value>           regular data element
  //   dn <count> <value>          regular data element without strobes
  //   s <count> <value>           regular BITSET element
  //   c <count> <value>           regular BITCLEAR element
  //   x <count> <value>           regular BITFLIP element
  //   n <count> <value>           regular BITNOT element
  //   a <count> <value>           regular BITAND element
  //   o <count> <value>           regular BITOR element
  //   xr <count> <value>          regular BITXOR element
  //   xn <count> <value>          regular BITXNOR element
  //   sl <count> <value>          regular BITSLL element
  //   sr <count> <value>          regular BITSRL element
  //   store <slot> <op> ...       store a regular element in preprocessor slot <slot>
  //                               where <op> is one of d, dn, s, c, x, n, a, o, xr, xn, sl, sr
  //   r <repetitions> <length>    replay a stored subsequence
  //   rt                          retrigger with the default final value
  //   pr <count>                  emit pseudo-random values for <count> cycles
  //   final <value>               terminate the sequence with the selected final output value
  //   t <pattern> <mask>          final trigger-condition element
  //   tn <pattern> <mask>         non-final trigger-condition element
  //   f                           request forced trigger instead of arm-and-wait
  // The returned boolean carries that forced-trigger request alongside the sequence.
  Sequence elements;
  bool force_trigger = false;
  auto parse_regular_args = [&f](const char *token_name) {
    std::string sc, sv;
    if (!(f >> sc >> sv))
      throw std::runtime_error(std::string("Incomplete '") + token_name + "' record: expected count and value");
    return std::make_pair(parse_count_t(sc), parse_value_t(sv));
  };

  auto parse_regular_element = [&parse_regular_args](const std::string &token_name) {
    if (token_name == "d") {
      auto [c, v] = parse_regular_args("d");
      return el(c, v);
    } else if (token_name == "dn") {
      auto [c, v] = parse_regular_args("dn");
      return el(NoStrobe(c), v);
    } else if (token_name == "s") {
      auto [c, v] = parse_regular_args("s");
      return el(c, BitSet(v));
    } else if (token_name == "c") {
      auto [c, v] = parse_regular_args("c");
      return el(c, BitClear(v));
    } else if (token_name == "x") {
      auto [c, v] = parse_regular_args("x");
      return el(c, BitFlip(v));
    } else if (token_name == "n") {
      auto [c, v] = parse_regular_args("n");
      return el(c, BitNot(v));
    } else if (token_name == "a") {
      auto [c, v] = parse_regular_args("a");
      return el(c, BitAnd(v));
    } else if (token_name == "o") {
      auto [c, v] = parse_regular_args("o");
      return el(c, BitOr(v));
    } else if (token_name == "xr") {
      auto [c, v] = parse_regular_args("xr");
      return el(c, BitXor(v));
    } else if (token_name == "xn") {
      auto [c, v] = parse_regular_args("xn");
      return el(c, BitXnor(v));
    } else if (token_name == "sl") {
      auto [c, v] = parse_regular_args("sl");
      return el(c, BitSll(v));
    } else if (token_name == "sr") {
      auto [c, v] = parse_regular_args("sr");
      return el(c, BitSrl(v));
    }
    throw std::runtime_error("Unknown regular sequence token in 'store': '" + token_name + "'");
  };

  while (f) {
    std::string token;
    f >> token;
    if (!f) break;

    if (token == "d" || token == "dn" || token == "s" || token == "c" || token == "x" ||
        token == "n" || token == "a" || token == "o" || token == "xr" || token == "xn" ||
        token == "sl" || token == "sr") {
      elements.push_back(parse_regular_element(token));
    } else if (token == "store") {
      std::string si, op;
      if (!(f >> si >> op))
        throw std::runtime_error("Incomplete 'store' record: expected slot and regular-element token");
      auto e = parse_regular_element(op);
      elements.push_back(e.store(parse_count_t(si)));
    } else if (token == "r") {
      std::string sr, slen;
      if (!(f >> sr >> slen))
        throw std::runtime_error("Incomplete 'r' record: expected repetitions and length");
      elements.push_back(el(Replay{}, parse_count_t(sr), parse_value_t(slen)));
    } else if (token == "rt") {
      elements.push_back(el(Retrig{}));
    } else if (token == "pr") {
      std::string sc;
      if (!(f >> sc))
        throw std::runtime_error("Incomplete 'pr' record: expected count");
      elements.push_back(el(PseudoRandom{}, parse_count_t(sc)));
    } else if (token == "final") {
      std::string sv;
      if (!(f >> sv))
        throw std::runtime_error("Incomplete 'final' record: expected value");
      elements.push_back(el(parse_value_t(sv)));
    } else if (token == "t") {
      std::string sp, sm;
      if (!(f >> sp >> sm))
        throw std::runtime_error("Incomplete 't' record: expected pattern and mask");
      auto p = parse_trigger_t(sp);
      auto m = parse_trigger_t(sm);
      elements.push_back(el(p, m, true));
    } else if (token == "tn") {
      std::string sp, sm;
      if (!(f >> sp >> sm))
        throw std::runtime_error("Incomplete 'tn' record: expected pattern and mask");
      auto p = parse_trigger_t(sp);
      auto m = parse_trigger_t(sm);
      elements.push_back(el(p, m, false));
    } else if (token == "f") {
      force_trigger = true;
    } else {
      throw std::runtime_error("Unknown sequence token: '" + token + "'");
    }
  }
  return {elements, force_trigger};
}
