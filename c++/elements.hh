// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Elements representing run-length-encoded data

#pragma once

#include <array>
#include <bitset>
#include <cstdint> // integer types, uint32_t, etc.
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "tidbit.hh"
#include "misc.hh"
#include "config.h"
#include "format.hh"

// Element types: regular (value update), trigger conditions, replay, sequence terminator, retrigger, random
enum class el_type { regular, trigger, replay, final, retrig, prng };
enum class counter_kind_t { plain, strobe, nostrobe };
enum class value_kind_t { plain, bitload, bitset, bitclear, bitflip, bitnot, bit_and, bit_or, bitxor, bitxnor, bitsll, bitsrl, trigger };

inline value_t apply_value_kind(value_kind_t kind, value_t operand, value_t previous) {
  switch (kind) {
    case value_kind_t::plain:
    case value_kind_t::bitload:
    case value_kind_t::trigger:
      return operand;
    case value_kind_t::bitset:
      return previous | operand;
    case value_kind_t::bitclear:
      return previous & ~operand;
    case value_kind_t::bitflip:
      return previous ^ operand;
    case value_kind_t::bitnot:
      return ~previous;
    case value_kind_t::bit_and:
      return operand & previous;
    case value_kind_t::bit_or:
      return operand | previous;
    case value_kind_t::bitxor:
      return operand ^ previous;
    case value_kind_t::bitxnor:
      return ~(operand ^ previous);
    case value_kind_t::bitsll:
      return sll(previous, operand);
    case value_kind_t::bitsrl:
      return srl(previous, operand);
  }
  throw std::runtime_error("Unknown value kind");
}

inline std::string format_count(count_t count) {
  std::stringstream ss;
  ss << "count=" << hex8(count) << " [" << dec13(count) << "]";
  return ss.str();
}

inline std::string format_value(value_t value) {
  std::stringstream ss;
  ss << "value=" << hex8(value) << " [" << dec13(value) << "]";
  return ss.str();
}

inline std::string format_trigger_value(value_t value) {
  const auto pattern = static_cast<trigger_t>(value & TRIGGER_MASK);
  const auto mask = static_cast<trigger_t>((value >> WIDTH_TRIGGER) & TRIGGER_MASK);
  std::stringstream ss;
  ss << "0x" << std::hex << int(value) << " "
     << "trig=0x" << std::hex << int(pattern) << " {" << std::bitset<WIDTH_TRIGGER>(int(pattern)) << "} "
     << "mask=0x" << std::hex << int(mask) << " {" << std::bitset<WIDTH_TRIGGER>(int(mask)) << "}";
  return ss.str();
}

inline constexpr char strobestring[] = "(strobe)";
inline constexpr char nostrobestring[] = "(no strobe)";
inline constexpr char finalstring[] = "(final)";
inline constexpr char retrigstring[] = "(retrig)";
inline constexpr char triggerstring[] = "(trigger)";
inline constexpr char prngstring[] = "(PRNG)";

// base wrapper class for storing the count value
class Counter {
protected:
  count_t c;
public:
  Counter(count_t _c) : c(_c) {}
  virtual ~Counter() = default;
  count_t count() const noexcept { return c; }
  virtual std::string count_str() const {
    return format_count(count());
  }
  virtual control_t control_bits() const { return 0; } // corresponding control bits (that need to be or'd in)
  virtual std::string desc() const { return ""; } // descriptive string
  virtual counter_kind_t kind() const { return counter_kind_t::plain; }
};

template<control_t ControlBits, counter_kind_t Kind, const char *DescString>
class TaggedCounter : public Counter {
public:
  using Counter::Counter;
  control_t control_bits() const override { return ControlBits; }
  std::string desc() const override { return DescString; }
  counter_kind_t kind() const override { return Kind; }
};

using Strobe = TaggedCounter<STROBE, counter_kind_t::strobe, strobestring>;

// wrapper class for storing the counter for elements that are emitted without strobing
using NoStrobe = TaggedCounter<NOSTROBE, counter_kind_t::nostrobe, nostrobestring>;

inline constexpr char bitloadstring[] = "(load)";
inline constexpr char bitsetstring[] = "(set)";
inline constexpr char bitclearstring[] = "(clear)";
inline constexpr char bitflipstring[] = "(flip)";
inline constexpr char bitnotstring[] = "(not)";
inline constexpr char bitandstring[] = "(and)";
inline constexpr char bitorstring[] = "(or)";
inline constexpr char bitxorstring[] = "(xor)";
inline constexpr char bitxnorstring[] = "(xnor)";
inline constexpr char bitsllstring[] = "(sll)";
inline constexpr char bitsrlstring[] = "(srl)";

// base wrapper class for storing binary values
class Value
{
protected:
  value_t v;
public:
  Value(value_t _v) : v(_v) {}
  virtual ~Value() = default;
  value_t value() const { return v; }
  virtual std::string value_str() const{
    return format_value(value());
  }
  virtual value_t result([[maybe_unused]] const value_t v_prev) const { return apply_value_kind(kind(), v, v_prev); }
  virtual control_t mode_bits() const { return 0; }
  virtual std::string desc() const { return ""; }
  virtual value_kind_t kind() const { return value_kind_t::plain; }
};

template<control_t ModeBits, value_kind_t Kind, const char *DescString>
class TaggedValue : public Value {
public:
  using Value::Value;
  control_t mode_bits() const override { return ModeBits; }
  std::string desc() const override { return DescString; }
  value_kind_t kind() const override { return Kind; }
};

using BitLoad = TaggedValue<BITLOAD, value_kind_t::bitload, bitloadstring>;
using BitSet = TaggedValue<BITSET, value_kind_t::bitset, bitsetstring>;
using BitClear = TaggedValue<BITCLEAR, value_kind_t::bitclear, bitclearstring>;
using BitFlip = TaggedValue<BITFLIP, value_kind_t::bitflip, bitflipstring>;
using BitNot = TaggedValue<BITNOT, value_kind_t::bitnot, bitnotstring>;
using BitAnd = TaggedValue<BITAND, value_kind_t::bit_and, bitandstring>;
using BitOr = TaggedValue<BITOR, value_kind_t::bit_or, bitorstring>;
using BitXor = TaggedValue<BITXOR, value_kind_t::bitxor, bitxorstring>;
using BitXnor = TaggedValue<BITXNOR, value_kind_t::bitxnor, bitxnorstring>;
using BitSll = TaggedValue<BITSLL, value_kind_t::bitsll, bitsllstring>;
using BitSrl = TaggedValue<BITSRL, value_kind_t::bitsrl, bitsrlstring>;

class TriggerCondition : public Value
{
private:
  bool final;
public:
  TriggerCondition(trigger_t pattern, trigger_t mask, bool _final) : Value(((value_t)mask << WIDTH_TRIGGER) + (value_t)pattern), final(_final) {}
  control_t mode_bits() const override { return TRIGGER | (final ? TRIGGERFINAL : 0); }
  std::string desc() const override { return std::string(triggerstring) + (final ? finalstring : ""); }
  value_kind_t kind() const override { return value_kind_t::trigger; }
  std::string value_str() const override { return format_trigger_value(v); }
};

// markers
struct Replay {};
struct Retrig {};
struct PseudoRandom{};

class el
{
private:
  struct regular_mode_spec {
    control_t mode;
    value_kind_t kind;
    const char *token;
  };

  control_t y;
  count_t c;
  value_t v;
  counter_kind_t counter_kind;
  value_kind_t value_kind;

  static constexpr std::array<regular_mode_spec, 11> regular_modes = {{
      {BITLOAD, value_kind_t::bitload, "d"},
      {BITSET, value_kind_t::bitset, "s"},
      {BITCLEAR, value_kind_t::bitclear, "c"},
      {BITFLIP, value_kind_t::bitflip, "x"},
      {BITNOT, value_kind_t::bitnot, "n"},
      {BITAND, value_kind_t::bit_and, "a"},
      {BITOR, value_kind_t::bit_or, "o"},
      {BITXOR, value_kind_t::bitxor, "xr"},
      {BITXNOR, value_kind_t::bitxnor, "xn"},
      {BITSLL, value_kind_t::bitsll, "sl"},
      {BITSRL, value_kind_t::bitsrl, "sr"},
  }};

  static const regular_mode_spec *find_regular_mode_by_mode(control_t control) {
    const control_t mode = control & MODEBITS;
    for (const auto &spec : regular_modes)
      if (spec.mode == mode)
        return &spec;
    return nullptr;
  }

  static const regular_mode_spec *find_regular_mode_by_token(const std::string &token) {
    for (const auto &spec : regular_modes)
      if (token == spec.token)
        return &spec;
    return nullptr;
  }

  static value_kind_t value_kind_from_mode_bits(control_t control) {
    if (const auto *spec = find_regular_mode_by_mode(control))
      return spec->kind;
    throw std::runtime_error("Unknown regular value mode");
  }

  static value_kind_t normalized_regular_value_kind(const Value &value) {
    return value.kind() == value_kind_t::plain ? value_kind_t::bitload : value.kind();
  }

  static control_t normalized_regular_mode_bits(const Value &value) {
    return normalized_regular_value_kind(value) == value_kind_t::bitload ? BITLOAD : value.mode_bits();
  }

  static el_type type_from_control(control_t control) {
    if ((control & TRIGGERBITS) == TRIGGER || (control & TRIGGERBITS) == (TRIGGER | TRIGGERFINAL))
      return el_type::trigger;
    if ((control & REPLAY) == REPLAY)
      return el_type::replay;
    if ((control & RETRIG) == RETRIG)
      return el_type::retrig;
    if ((control & PRNG) == PRNG)
      return el_type::prng;
    if ((control & TERMINATE) == TERMINATE)
      return el_type::final;
    return el_type::regular;
  }

  void sync_cached_state_from_control() {
    if (classify_control(y) == el_type::regular) {
      if ((y & NOSTROBE) == NOSTROBE) {
        counter_kind = counter_kind_t::nostrobe;
      } else if (counter_kind == counter_kind_t::nostrobe) {
        counter_kind = counter_kind_t::plain;
      }
      value_kind = value_kind_from_mode_bits(y);
    }
  }

  static std::string count_str(count_t count) {
    return format_count(count);
  }

  static std::string value_str(value_t value) {
    return format_value(value);
  }

  static std::string counter_desc(counter_kind_t kind) {
    switch (kind) {
      case counter_kind_t::plain: return "";
      case counter_kind_t::strobe: return strobestring;
      case counter_kind_t::nostrobe: return nostrobestring;
    }
    throw std::runtime_error("Unknown counter kind");
  }

  static std::string value_desc(value_kind_t kind) {
    switch (kind) {
      case value_kind_t::plain: return "";
      case value_kind_t::bitload: return bitloadstring;
      case value_kind_t::bitset: return bitsetstring;
      case value_kind_t::bitclear: return bitclearstring;
      case value_kind_t::bitflip: return bitflipstring;
      case value_kind_t::bitnot: return bitnotstring;
      case value_kind_t::bit_and: return bitandstring;
      case value_kind_t::bit_or: return bitorstring;
      case value_kind_t::bitxor: return bitxorstring;
      case value_kind_t::bitxnor: return bitxnorstring;
      case value_kind_t::bitsll: return bitsllstring;
      case value_kind_t::bitsrl: return bitsrlstring;
      case value_kind_t::trigger: return triggerstring;
    }
    throw std::runtime_error("Unknown value kind");
  }

  std::string trigger_value_str() const {
    return format_trigger_value(v);
  }

  std::string trigger_desc() const {
    return std::string(triggerstring) + (trigger_is_final() ? finalstring : "");
  }

  value_t apply_value(value_t previous) const {
    return apply_value_kind(value_kind, v, previous);
  }

  static el regular_from_control(control_t control, count_t count, value_t value) {
    return el(control, count, value,
              no_strobe_from_control(control) ? counter_kind_t::nostrobe : counter_kind_t::plain,
              value_kind_from_mode_bits(control));
  }

  el(control_t control, count_t count, value_t value, counter_kind_t counter_kind_, value_kind_t value_kind_)
      : y(control),
        c(count),
        v(value),
        counter_kind(counter_kind_),
        value_kind(value_kind_) {
    sync_cached_state_from_control();
  }

public:
  // Sequence terminator
  el(value_t _v = default_final_value)
      : el(TERMINATE, 1, _v, counter_kind_t::plain, value_kind_t::plain) {}
  static el final_with_value(const Value &_vv) {
    return el(static_cast<control_t>(TERMINATE | _vv.mode_bits()),
              1,
              _vv.value(),
              counter_kind_t::plain,
              _vv.kind());
  }
  // Regular element
  el(count_t _c, value_t _v)
      : el(BITLOAD, _c, _v, counter_kind_t::plain, value_kind_t::bitload) {};
  el(const Counter &_cc, value_t _v)
      : el(static_cast<control_t>(_cc.control_bits() | BITLOAD),
           _cc.count(), _v, _cc.kind(), value_kind_t::bitload) {};
  el(const Counter &_cc, const Value &_vv)
      : el(static_cast<control_t>(_cc.control_bits() | normalized_regular_mode_bits(_vv)),
           _cc.count(), _vv.value(), _cc.kind(), normalized_regular_value_kind(_vv)) {};
  // Trigger element
  el(trigger_t pattern, trigger_t mask, bool final)
      : el(static_cast<control_t>(TRIGGER | (final ? TRIGGERFINAL : 0)),
           0,
           static_cast<value_t>((static_cast<value_t>(mask) << WIDTH_TRIGGER) + static_cast<value_t>(pattern)),
           counter_kind_t::plain,
           value_kind_t::trigger) {}
  // Repetitions
  el(Replay, count_t repetitions, value_t length)
      : el(REPLAY, repetitions, length, counter_kind_t::plain, value_kind_t::plain) {}
  // Retriggering
  el(Retrig, value_t _v = default_final_value)
      : el(RETRIG, 1, _v, counter_kind_t::plain, value_kind_t::plain) {}
  // Pseudo random
  el(PseudoRandom, count_t _c)
      : el(PRNG, _c, 0, counter_kind_t::plain, value_kind_t::plain) {}

  static el_type classify_control(control_t control) { return type_from_control(control); }
  static control_t mode_from_control(control_t control) { return control & MODEBITS; }
  static bool no_strobe_from_control(control_t control) { return (control & NOSTROBE) == NOSTROBE; }
  static bool stored_from_control(control_t control) { return (control & STORE) == STORE; }
  static size_t store_slot_from_control(control_t control) {
    if (!stored_from_control(control))
      throw std::runtime_error("Element is not marked for storage");
    return (control & POSITIONS_MASK) >> SHIFT_POSITION;
  }
  static trigger_t trigger_pattern_from_value(value_t value) { return static_cast<trigger_t>(value & TRIGGER_MASK); }
  static trigger_t trigger_mask_from_value(value_t value) { return static_cast<trigger_t>((value >> WIDTH_TRIGGER) & TRIGGER_MASK); }
  static bool trigger_final_from_control(control_t control) { return (control & TRIGGERFINAL) == TRIGGERFINAL; }
  static control_t regular_control_from_token(const std::string &token, const char *context = nullptr) {
    if (token == "dn")
      return static_cast<control_t>(BITLOAD | NOSTROBE);
    if (const auto *spec = find_regular_mode_by_token(token))
      return spec->mode;

    if (context != nullptr)
      throw std::runtime_error("Unknown regular sequence token in '" + std::string(context) + "': '" + token + "'");
    throw std::runtime_error("Unknown regular sequence token: '" + token + "'");
  }
  static std::string regular_token_from_control(control_t control) {
    const bool no_strobe = no_strobe_from_control(control);
    const auto *spec = find_regular_mode_by_mode(control);

    if (spec == nullptr)
      throw std::runtime_error("Unsupported regular element mode in text sequence writer");

    if (no_strobe && spec->mode != BITLOAD)
      throw std::runtime_error("Text sequence writer does not support non-BITLOAD no-strobe elements");

    if (spec->mode == BITLOAD)
      return no_strobe ? "dn" : "d";
    return spec->token;
  }
  static bool is_regular_token(const std::string &token) {
    return token == "dn" || find_regular_mode_by_token(token) != nullptr;
  }
  static el from_raw_triplet(control_t control, count_t count, value_t value) {
    switch (classify_control(control)) {
      case el_type::trigger:
        return el(control, 0, value, counter_kind_t::plain, value_kind_t::trigger);
      case el_type::replay:
        return el(control, count, value, counter_kind_t::plain, value_kind_t::plain);
      case el_type::retrig:
        return el(control, 1, value, counter_kind_t::plain, value_kind_t::plain);
      case el_type::prng:
        return el(control, count, 0, counter_kind_t::plain, value_kind_t::plain);
      case el_type::final:
        return el(control, 1, value, counter_kind_t::plain, value_kind_from_mode_bits(control));
      case el_type::regular:
        return regular_from_control(control, count, value);
    }

    throw std::runtime_error("Unsupported element control kind in binary sequence reader");
  }
  static el from_regular_token(const std::string &token, count_t count, value_t value, const char *context = nullptr) {
    return from_raw_triplet(regular_control_from_token(token, context), count, value);
  }

  control_t control() const { return y; }
  count_t count() const { return c; }
  value_t value() const { return v; }
  el_type kind() const { return classify_control(y); }
  control_t mode() const { return mode_from_control(y); }
  bool no_strobe() const { return no_strobe_from_control(y); }
  std::string regular_token() const {
    if (!is_regular())
      throw std::runtime_error("regular_token() requires a regular element");
    return regular_token_from_control(y);
  }
  bool is_stored() const { return stored_from_control(y); }
  size_t store_slot() const {
    return store_slot_from_control(y);
  }
  trigger_t trigger_pattern() const { return trigger_pattern_from_value(v); }
  trigger_t trigger_mask() const { return trigger_mask_from_value(v); }
  bool trigger_is_final() const { return trigger_final_from_control(y); }

  // Mark an element for storage in fast memory register i
  el stored_in(unsigned int i) const {
    if (i >= POSITIONS)
      throw std::runtime_error("store() out of bounds");
    el copy = *this;
    copy.y = static_cast<control_t>(copy.y | STORE | (i << SHIFT_POSITION));
    return copy;
  }

  el& store(unsigned int i) {
    *this = stored_in(i);
    return *this;
  }

  el with_control(control_t control) const {
    el copy = *this;
    copy.y = control;
    copy.sync_cached_state_from_control();
    return copy;
  }

  el with_count(count_t count) const {
    el copy = *this;
    copy.c = count;
    return copy;
  }

  el with_counter(const Counter &counter) const {
    el copy = *this;
    copy.c = counter.count();
    copy.counter_kind = counter.kind();
    copy.y = static_cast<control_t>((copy.y & ~NOSTROBE) | counter.control_bits());
    copy.sync_cached_state_from_control();
    return copy;
  }

  el with_regular_value(const Value &value) const {
    if (!is_regular())
      throw std::runtime_error("with_regular_value() requires a regular element");
    el copy = *this;
    copy.v = value.value();
    copy.value_kind = normalized_regular_value_kind(value);
    copy.y = static_cast<control_t>((copy.y & ~(MODEBITS | TRIGGERBITS)) | normalized_regular_mode_bits(value));
    copy.sync_cached_state_from_control();
    return copy;
  }

  el as_bitload_after(value_t previous_value) const {
    if (!is_regular())
      return *this;
    return with_regular_value(BitLoad(updated_value(previous_value)));
  }

  void set_control(control_t _y) {
    *this = with_control(_y);
  }
  void set_count(count_t _c) { *this = with_count(_c); }
  void set_count(const Counter &_cc) {
    *this = with_counter(_cc);
  }
  void set_value(const Value &_vv) {
    if (is_regular()) {
      *this = with_regular_value(_vv);
      return;
    }

    v = _vv.value();
    value_kind = _vv.kind();
    y = static_cast<control_t>((y & ~(MODEBITS | TRIGGERBITS)) | _vv.mode_bits());
    sync_cached_state_from_control();
  }

  // The resulting data value if the previous value was v_prev and the value was updated according to the contained Value object
  value_t updated_value(const value_t v_prev) const { return apply_value(v_prev); }

  bool is_regular() const { return kind() == el_type::regular; }
  bool is_trigger() const { return kind() == el_type::trigger; }
  bool is_replay() const { return kind() == el_type::replay; }
  bool is_final() const { return kind() == el_type::final; }
  bool is_retrig() const { return kind() == el_type::retrig; }
  bool is_prng() const { return kind() == el_type::prng; }

  // Additional info about control bits
  std::string decode() const {
    std::stringstream s;
    if (is_stored())
      s << " (store " << store_slot() << ")";
    return s.str();
  }

  std::string desc() const {
    std::stringstream ss;
    if (is_regular()) {
      ss << value_str(v) << " " << count_str(c) << " " << counter_desc(counter_kind) << " " << value_desc(value_kind);
      ss << " control=0x" << std::hex << control() << decode();
    } else if (is_trigger()) {
      ss << trigger_value_str() << " " << trigger_desc();
    } else if (is_replay()) {
      ss << "Replay: repetitions=" << std::dec << count() << " length=" << std::dec << value();
    } else if (is_final()) {
      ss << finalstring;
    } else if (is_retrig()) {
      ss << retrigstring;
    } else if (is_prng()) {
      ss << prngstring << " length=" << std::dec << count();
    } else {
      throw std::runtime_error("Unknown element kind");
    }
    return ss.str();
  }

  std::string sequence_record() const {
    std::ostringstream out;
    if (is_regular()) {
      const auto token = regular_token();
      if (is_stored()) {
        out << "store " << std::dec << store_slot() << " " << token
            << " " << std::dec << count()
            << " 0x" << std::hex << value();
      } else {
        out << token
            << " " << std::dec << count()
            << " 0x" << std::hex << value();
      }
    } else if (is_trigger()) {
      out << (trigger_is_final() ? "t" : "tn")
          << " 0x" << std::hex << int(trigger_pattern())
          << " 0x" << std::hex << int(trigger_mask());
    } else if (is_final()) {
      if (mode() != BITLOAD)
        throw std::runtime_error("Text sequence writer does not support non-BITLOAD final elements");
      out << "final 0x" << std::hex << value();
    } else if (is_replay()) {
      out << "r"
          << " " << std::dec << count()
          << " 0x" << std::hex << value();
    } else if (is_retrig()) {
      out << "rt";
    } else if (is_prng()) {
      out << "pr"
          << " " << std::dec << count();
    } else {
      throw std::runtime_error("Unsupported element in text sequence writer");
    }
    return out.str();
  }

  friend inline std::ostream &operator<<(std::ostream &o, const el &e) {
    o << e.desc();
    return o;
  }

  // We only compare the values, the type of stored objects is ignored.
  friend inline bool operator==(const el &X, const el &Y) {
    return X.y == Y.y && X.c == Y.c && X.v == Y.v;
  }

  friend inline bool operator!=(const el &X, const el &Y) { return !(X == Y); }
};
