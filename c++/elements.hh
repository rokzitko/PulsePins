// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Elements representing run-length-encoded data

#pragma once

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
    std::stringstream ss;
    ss << "count=" << hex8(count()) << " [" << dec13(count()) << "]";
    return ss.str();
  }
  virtual control_t control_bits() const { return 0; } // corresponding control bits (that need to be or'd in)
  virtual std::string desc() const { return ""; } // descriptive string
  virtual counter_kind_t kind() const { return counter_kind_t::plain; }
};

// wrapper class for storing the counter for elements that need to be strobed out
class Strobe : public Counter
{
public:
  Strobe(count_t c) : Counter(c) {}
  control_t control_bits() const override { return STROBE; }
  std::string desc() const override { return strobestring; }
  counter_kind_t kind() const override { return counter_kind_t::strobe; }
};

// wrapper class for storing the counter for elements that are emitted without strobing
class NoStrobe : public Counter
{
public:
  NoStrobe(count_t c) : Counter(c) {}
  control_t control_bits() const override { return NOSTROBE; }
  std::string desc() const override { return nostrobestring; }
  counter_kind_t kind() const override { return counter_kind_t::nostrobe; }
};

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
    std::stringstream ss;
    ss << "value=" << hex8(value()) << " [" << dec13(value()) << "]";
    return ss.str();
  }
  virtual value_t result([[maybe_unused]] const value_t v_prev) const { return v; }
  virtual control_t mode_bits() const { return 0; }
  virtual std::string desc() const { return ""; }
  virtual value_kind_t kind() const { return value_kind_t::plain; }
};

// wrapper class for storing a bit setting pattern
class BitLoad : public Value
{
public:
  BitLoad(value_t _v) : Value(_v) {}
  value_t result([[maybe_unused]] const value_t v_prev) const override { return v; }
  control_t mode_bits() const override { return BITLOAD; }
  std::string desc() const override { return bitloadstring; }
  value_kind_t kind() const override { return value_kind_t::bitload; }
};

// wrapper class for storing a bit setting pattern
class BitSet : public Value
{
public:
  BitSet(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v_prev | v; }
  control_t mode_bits() const override { return BITSET; }
  std::string desc() const override { return bitsetstring; }
  value_kind_t kind() const override { return value_kind_t::bitset; }
};

// wrapper class for storing a bit clearing pattern
class BitClear : public Value
{
public:
  BitClear(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v_prev & ~v; }
  control_t mode_bits() const override { return BITCLEAR; }
  std::string desc() const override { return bitclearstring; }
  value_kind_t kind() const override { return value_kind_t::bitclear; }
};

// wrapper class for string a bit flipping pattern
class BitFlip : public Value
{
public:
  BitFlip(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v_prev ^ v; }
  control_t mode_bits() const override { return BITFLIP; }
  std::string desc() const override { return bitflipstring; }
  value_kind_t kind() const override { return value_kind_t::bitflip; }
};

class BitNot : public Value
{
public:
  BitNot(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return ~v_prev; }
  control_t mode_bits() const override { return BITNOT; }
  std::string desc() const override { return bitnotstring; }
  value_kind_t kind() const override { return value_kind_t::bitnot; }
};

class BitAnd : public Value
{
public:
  BitAnd(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v & v_prev; }
  control_t mode_bits() const override { return BITAND; }
  std::string desc() const override { return bitandstring; }
  value_kind_t kind() const override { return value_kind_t::bit_and; }
};

class BitOr : public Value
{
public:
  BitOr(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v | v_prev; }
  control_t mode_bits() const override { return BITOR; }
  std::string desc() const override { return bitorstring; }
  value_kind_t kind() const override { return value_kind_t::bit_or; }
};

class BitXor : public Value
{
public:
  BitXor(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v ^ v_prev; }
  control_t mode_bits() const override { return BITXOR; }
  std::string desc() const override { return bitxorstring; }
  value_kind_t kind() const override { return value_kind_t::bitxor; }
};

class BitXnor : public Value
{
public:
  BitXnor(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return ~(v ^ v_prev); }
  control_t mode_bits() const override { return BITXNOR; }
  std::string desc() const override { return bitxnorstring; }
  value_kind_t kind() const override { return value_kind_t::bitxnor; }
};

class BitSll : public Value
{
public:
  BitSll(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return sll(v_prev, v); }
  control_t mode_bits() const override { return BITSLL; }
  std::string desc() const override { return bitsllstring; }
  value_kind_t kind() const override { return value_kind_t::bitsll; }
};

class BitSrl : public Value
{
public:
  BitSrl(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return srl(v_prev, v); }
  control_t mode_bits() const override { return BITSRL; }
  std::string desc() const override { return bitsrlstring; }
  value_kind_t kind() const override { return value_kind_t::bitsrl; }
};

class TriggerCondition : public Value
{
private:
  bool final;
public:
  TriggerCondition(trigger_t pattern, trigger_t mask, bool _final) : Value(((value_t)mask << WIDTH_TRIGGER) + (value_t)pattern), final(_final) {}
  control_t mode_bits() const override { return TRIGGER | (final ? TRIGGERFINAL : 0); }
  std::string desc() const override { return std::string(triggerstring) + (final ? finalstring : ""); }
  value_kind_t kind() const override { return value_kind_t::trigger; }
  std::string value_str() const override {
    std::stringstream ss;
    trigger_t pattern = v & TRIGGER_MASK;
    trigger_t mask = (v >> WIDTH_TRIGGER) & TRIGGER_MASK;
    ss << "0x" << std::hex << int(v) << " "
        << "trig=0x" << std::hex << int(pattern) << " {" << std::bitset<WIDTH_TRIGGER>(int(pattern)) << "} "
        << "mask=0x" << std::hex << int(mask)    << " {" << std::bitset<WIDTH_TRIGGER>(int(mask))    << "}";
    return ss.str();
  }
};

// markers
struct Replay {};
struct Retrig {};
struct PseudoRandom{};

class el
{
private:
  el_type t; // regular, trigger or final(terminator)
  control_t y;
  count_t c;
  value_t v;
  counter_kind_t counter_kind;
  value_kind_t value_kind;

  static value_kind_t value_kind_from_mode_bits(control_t control, value_kind_t fallback) {
    switch (control & MODEBITS) {
      case BITLOAD:
        return fallback == value_kind_t::plain ? value_kind_t::plain : value_kind_t::bitload;
      case BITSET:
        return value_kind_t::bitset;
      case BITCLEAR:
        return value_kind_t::bitclear;
      case BITFLIP:
        return value_kind_t::bitflip;
      case BITNOT:
        return value_kind_t::bitnot;
      case BITAND:
        return value_kind_t::bit_and;
      case BITOR:
        return value_kind_t::bit_or;
      case BITXOR:
        return value_kind_t::bitxor;
      case BITXNOR:
        return value_kind_t::bitxnor;
      case BITSLL:
        return value_kind_t::bitsll;
      case BITSRL:
        return value_kind_t::bitsrl;
      default:
        throw std::runtime_error("Unknown regular value mode");
    }
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
    t = type_from_control(y);
    if (t == el_type::regular) {
      if ((y & NOSTROBE) == NOSTROBE) {
        counter_kind = counter_kind_t::nostrobe;
      } else if (counter_kind == counter_kind_t::nostrobe) {
        counter_kind = counter_kind_t::plain;
      }
      value_kind = value_kind_from_mode_bits(y, value_kind);
    }
  }

  static std::string count_str(count_t count) {
    std::stringstream ss;
    ss << "count=" << hex8(count) << " [" << dec13(count) << "]";
    return ss.str();
  }

  static std::string value_str(value_t value) {
    std::stringstream ss;
    ss << "value=" << hex8(value) << " [" << dec13(value) << "]";
    return ss.str();
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
    std::stringstream ss;
    const trigger_t pattern = trigger_pattern();
    const trigger_t mask = trigger_mask();
    ss << "0x" << std::hex << int(v) << " "
       << "trig=0x" << std::hex << int(pattern) << " {" << std::bitset<WIDTH_TRIGGER>(int(pattern)) << "} "
       << "mask=0x" << std::hex << int(mask) << " {" << std::bitset<WIDTH_TRIGGER>(int(mask)) << "}";
    return ss.str();
  }

  std::string trigger_desc() const {
    return std::string(triggerstring) + (trigger_is_final() ? finalstring : "");
  }

  value_t apply_value(value_t previous) const {
    switch (value_kind) {
      case value_kind_t::plain:
      case value_kind_t::bitload:
      case value_kind_t::trigger:
        return v;
      case value_kind_t::bitset:
        return previous | v;
      case value_kind_t::bitclear:
        return previous & ~v;
      case value_kind_t::bitflip:
        return previous ^ v;
      case value_kind_t::bitnot:
        return ~previous;
      case value_kind_t::bit_and:
        return v & previous;
      case value_kind_t::bit_or:
        return v | previous;
      case value_kind_t::bitxor:
        return v ^ previous;
      case value_kind_t::bitxnor:
        return ~(v ^ previous);
      case value_kind_t::bitsll:
        return sll(previous, v);
      case value_kind_t::bitsrl:
        return srl(previous, v);
    }
    throw std::runtime_error("Unknown value kind");
  }

public:
  // General constructor, called internally by other constructors, less appropriate for general use
  el(el_type _t, const Counter &_cc, const Value &_vv, control_t _y = 0)
      : t(_t),
        y(_cc.control_bits() | _vv.mode_bits() | _y),
        c(_cc.count()),
        v(_vv.value()),
        counter_kind(_cc.kind()),
        value_kind(_vv.kind()) {
  }
  // Sequence terminator
  el(value_t _v = default_final_value) : el(el_type::final, Counter(1), Value(_v), TERMINATE) {}
  // Regular element
  el(count_t _c, value_t _v) : el(el_type::regular, Counter(_c), BitLoad(_v)) {};
  el(const Counter &_cc, value_t _v) : el(el_type::regular, _cc, BitLoad(_v)) {};
  el(const Counter &_cc, const Value &_vv) : el(el_type::regular, _cc, _vv) {};
  // Trigger element
  el(trigger_t pattern, trigger_t mask, bool final) : el(el_type::trigger, Counter(0), TriggerCondition(pattern, mask, final), final ? TRIGGERFINAL : 0) {}
  // Repetitions
  el(Replay, count_t repetitions, value_t length) : el(el_type::replay, Counter(repetitions), Value(length), REPLAY) {}
  // Retriggering
  el(Retrig, value_t _v = default_final_value) : el(el_type::retrig, Counter(1), Value(_v), RETRIG) {}
  // Pseudo random
  el(PseudoRandom, count_t _c) : el(el_type::prng, Counter(_c), Value(0), PRNG) {}

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

  control_t control() const { return y; }
  count_t count() const { return c; }
  value_t value() const { return v; }
  control_t mode() const { return mode_from_control(y); }
  bool no_strobe() const { return no_strobe_from_control(y); }
  bool is_stored() const { return stored_from_control(y); }
  size_t store_slot() const {
    return store_slot_from_control(y);
  }
  trigger_t trigger_pattern() const { return trigger_pattern_from_value(v); }
  trigger_t trigger_mask() const { return trigger_mask_from_value(v); }
  bool trigger_is_final() const { return trigger_final_from_control(y); }

  // Mark an element for storage in fast memory register i
  el& store(unsigned int i) {
    if (i >= POSITIONS)
      throw std::runtime_error("store() out of bounds");
    y |= STORE + (i << SHIFT_POSITION);
    return *this;
  }

  void set_control(control_t _y) {
    y = _y;
    sync_cached_state_from_control();
  }
  void set_count(count_t _c) { c = _c; }
  void set_count(const Counter &_cc) {
    c = _cc.count();
    counter_kind = _cc.kind();
    y = (y & ~NOSTROBE) | _cc.control_bits();
  }
  void set_value(const Value &_vv) {
    v = _vv.value();
    value_kind = _vv.kind();
    y = (y & ~(MODEBITS | TRIGGERBITS)) | _vv.mode_bits();
    sync_cached_state_from_control();
  }

  // The resulting data value if the previous value was v_prev and the value was updated according to the contained Value object
  value_t updated_value(const value_t v_prev) const { return apply_value(v_prev); }

  bool is_regular() const { return t == el_type::regular; }
  bool is_trigger() const { return t == el_type::trigger; }
  bool is_replay() const { return t == el_type::replay; }
  bool is_final() const { return t == el_type::final; }
  bool is_retrig() const { return t == el_type::retrig; }
  bool is_prng() const { return t == el_type::prng; }

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
