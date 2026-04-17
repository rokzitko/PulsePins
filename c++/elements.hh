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
};

// wrapper class for storing the counter for elements that need to be strobed out
class Strobe : public Counter
{
public:
  Strobe(count_t c) : Counter(c) {}
  control_t control_bits() const override { return STROBE; }
  std::string desc() const override { return strobestring; }
};

// wrapper class for storing the counter for elements that are emitted without strobing
class NoStrobe : public Counter
{
public:
  NoStrobe(count_t c) : Counter(c) {}
  control_t control_bits() const override { return NOSTROBE; }
  std::string desc() const override { return nostrobestring; }
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
};

// wrapper class for storing a bit setting pattern
class BitLoad : public Value
{
public:
  BitLoad(value_t _v) : Value(_v) {}
  value_t result([[maybe_unused]] const value_t v_prev) const override { return v; }
  control_t mode_bits() const override { return BITLOAD; }
  std::string desc() const override { return bitloadstring; }
};

// wrapper class for storing a bit setting pattern
class BitSet : public Value
{
public:
  BitSet(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v_prev | v; }
  control_t mode_bits() const override { return BITSET; }
  std::string desc() const override { return bitsetstring; }
};

// wrapper class for storing a bit clearing pattern
class BitClear : public Value
{
public:
  BitClear(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v_prev & ~v; }
  control_t mode_bits() const override { return BITCLEAR; }
  std::string desc() const override { return bitclearstring; }
};

// wrapper class for string a bit flipping pattern
class BitFlip : public Value
{
public:
  BitFlip(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v_prev ^ v; }
  control_t mode_bits() const override { return BITFLIP; }
  std::string desc() const override { return bitflipstring; }
};

class BitNot : public Value
{
public:
  BitNot(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return ~v_prev; }
  control_t mode_bits() const override { return BITNOT; }
  std::string desc() const override { return bitnotstring; }
};

class BitAnd : public Value
{
public:
  BitAnd(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v & v_prev; }
  control_t mode_bits() const override { return BITAND; }
  std::string desc() const override { return bitandstring; }
};

class BitOr : public Value
{
public:
  BitOr(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v | v_prev; }
  control_t mode_bits() const override { return BITOR; }
  std::string desc() const override { return bitorstring; }
};

class BitXor : public Value
{
public:
  BitXor(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v ^ v_prev; }
  control_t mode_bits() const override { return BITXOR; }
  std::string desc() const override { return bitxorstring; }
};

class BitXnor : public Value
{
public:
  BitXnor(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return ~(v ^ v_prev); }
  control_t mode_bits() const override { return BITXNOR; }
  std::string desc() const override { return bitxnorstring; }
};

class BitSll : public Value
{
public:
  BitSll(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return sll(v_prev, v); }
  control_t mode_bits() const override { return BITSLL; }
  std::string desc() const override { return bitsllstring; }
};

class BitSrl : public Value
{
public:
  BitSrl(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return srl(v_prev, v); }
  control_t mode_bits() const override { return BITSRL; }
  std::string desc() const override { return bitsrlstring; }
};

class TriggerCondition : public Value
{
private:
  bool final;
public:
  TriggerCondition(trigger_t pattern, trigger_t mask, bool _final) : Value(((value_t)mask << WIDTH_TRIGGER) + (value_t)pattern), final(_final) {}
  control_t mode_bits() const override { return TRIGGER | (final ? TRIGGERFINAL : 0); }
  std::string desc() const override { return std::string(triggerstring) + (final ? finalstring : ""); }
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
  enum class CounterKind { plain, strobe, nostrobe };
  enum class ValueKind { plain, bitload, bitset, bitclear, bitflip, bitnot, bit_and, bit_or, bitxor, bitxnor, bitsll, bitsrl, trigger };

  el_type t; // regular, trigger or final(terminator)
  control_t y;
  count_t c;
  value_t v;
  CounterKind counter_kind;
  ValueKind value_kind;

  static CounterKind counter_kind_from(const Counter &counter) {
    if (dynamic_cast<const NoStrobe *>(&counter))
      return CounterKind::nostrobe;
    if (dynamic_cast<const Strobe *>(&counter))
      return CounterKind::strobe;
    return CounterKind::plain;
  }

  static ValueKind value_kind_from(const Value &value) {
    if (dynamic_cast<const TriggerCondition *>(&value))
      return ValueKind::trigger;
    if (dynamic_cast<const BitLoad *>(&value))
      return ValueKind::bitload;
    if (dynamic_cast<const BitSet *>(&value))
      return ValueKind::bitset;
    if (dynamic_cast<const BitClear *>(&value))
      return ValueKind::bitclear;
    if (dynamic_cast<const BitFlip *>(&value))
      return ValueKind::bitflip;
    if (dynamic_cast<const BitNot *>(&value))
      return ValueKind::bitnot;
    if (dynamic_cast<const BitAnd *>(&value))
      return ValueKind::bit_and;
    if (dynamic_cast<const BitOr *>(&value))
      return ValueKind::bit_or;
    if (dynamic_cast<const BitXor *>(&value))
      return ValueKind::bitxor;
    if (dynamic_cast<const BitXnor *>(&value))
      return ValueKind::bitxnor;
    if (dynamic_cast<const BitSll *>(&value))
      return ValueKind::bitsll;
    if (dynamic_cast<const BitSrl *>(&value))
      return ValueKind::bitsrl;
    return ValueKind::plain;
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

  static std::string counter_desc(CounterKind kind) {
    switch (kind) {
      case CounterKind::plain: return "";
      case CounterKind::strobe: return strobestring;
      case CounterKind::nostrobe: return nostrobestring;
    }
    throw std::runtime_error("Unknown counter kind");
  }

  static std::string value_desc(ValueKind kind) {
    switch (kind) {
      case ValueKind::plain: return "";
      case ValueKind::bitload: return bitloadstring;
      case ValueKind::bitset: return bitsetstring;
      case ValueKind::bitclear: return bitclearstring;
      case ValueKind::bitflip: return bitflipstring;
      case ValueKind::bitnot: return bitnotstring;
      case ValueKind::bit_and: return bitandstring;
      case ValueKind::bit_or: return bitorstring;
      case ValueKind::bitxor: return bitxorstring;
      case ValueKind::bitxnor: return bitxnorstring;
      case ValueKind::bitsll: return bitsllstring;
      case ValueKind::bitsrl: return bitsrlstring;
      case ValueKind::trigger: return triggerstring;
    }
    throw std::runtime_error("Unknown value kind");
  }

  std::string trigger_value_str() const {
    std::stringstream ss;
    trigger_t pattern = v & TRIGGER_MASK;
    trigger_t mask = (v >> WIDTH_TRIGGER) & TRIGGER_MASK;
    ss << "0x" << std::hex << int(v) << " "
       << "trig=0x" << std::hex << int(pattern) << " {" << std::bitset<WIDTH_TRIGGER>(int(pattern)) << "} "
       << "mask=0x" << std::hex << int(mask) << " {" << std::bitset<WIDTH_TRIGGER>(int(mask)) << "}";
    return ss.str();
  }

  std::string trigger_desc() const {
    return std::string(triggerstring) + (((y & TRIGGERFINAL) == TRIGGERFINAL) ? finalstring : "");
  }

  value_t apply_value(value_t previous) const {
    switch (value_kind) {
      case ValueKind::plain:
      case ValueKind::bitload:
      case ValueKind::trigger:
        return v;
      case ValueKind::bitset:
        return previous | v;
      case ValueKind::bitclear:
        return previous & ~v;
      case ValueKind::bitflip:
        return previous ^ v;
      case ValueKind::bitnot:
        return ~previous;
      case ValueKind::bit_and:
        return v & previous;
      case ValueKind::bit_or:
        return v | previous;
      case ValueKind::bitxor:
        return v ^ previous;
      case ValueKind::bitxnor:
        return ~(v ^ previous);
      case ValueKind::bitsll:
        return sll(previous, v);
      case ValueKind::bitsrl:
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
        counter_kind(counter_kind_from(_cc)),
        value_kind(value_kind_from(_vv)) {
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

  control_t control() const { return y; }
  count_t count() const { return c; }
  value_t value() const { return v; }

  // Mark an element for storage in fast memory register i
  el& store(unsigned int i) {
    if (i >= POSITIONS)
      throw std::runtime_error("store() out of bounds");
    y |= STORE + (i << SHIFT_POSITION);
    return *this;
  }

  void set_control(control_t _y) { y = _y; }
  void set_count(count_t _c) { c = _c; }
  void set_count(const Counter &_cc) {
    c = _cc.count();
    counter_kind = counter_kind_from(_cc);
    y = (y & ~NOSTROBE) | _cc.control_bits();
  }
  void set_value(const Value &_vv) {
    v = _vv.value();
    value_kind = value_kind_from(_vv);
    y = (y & ~(MODEBITS | TRIGGERBITS)) | _vv.mode_bits();
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
    if ((y & STORE) == STORE) {
      size_t i = (y & POSITIONS_MASK) >> SHIFT_POSITION;
      s << " (store " << i << ")";
    }
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
