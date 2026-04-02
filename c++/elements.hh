// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Elements representing run-length-encoded data

#pragma once

#include <bitset>
#include <cstdint> // integer types, uint32_t, etc.
#include <iostream>
#include <memory> // shared_ptr
#include <sstream>
#include <stdexcept>
#include <string>

#include "tidbit.hh"
#include "misc.hh"
#include "config.h"
#include "format.hh"

// Element types: regular (value update), trigger conditions, replay, sequence terminator, retrigger, random
enum class el_type { regular, trigger, replay, final, retrig, prng };

const std::string strobestring = "(strobe)"s;
const std::string nostrobestring = "(no strobe)"s;
const std::string finalstring = "(final)"s;
const std::string retrigstring = "(retrig)"s;
const std::string triggerstring = "(trigger)"s;
const std::string prngstring = "(PRNG)"s;

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
  virtual std::shared_ptr<Counter> clone() const { return std::make_shared<Counter>(*this); } // deep-copy semantics, because copy is cheap
};

// wrapper class for storing the counter for elements that need to be strobed out
class Strobe : public Counter
{
public:
  Strobe(count_t c) : Counter(c) {}
  control_t control_bits() const override { return STROBE; }
  std::string desc() const override { return strobestring; }
  std::shared_ptr<Counter> clone() const override { return std::make_shared<Strobe>(*this); }
};

// wrapper class for storing the counter for elements that are emitted without strobing
class NoStrobe : public Counter
{
public:
  NoStrobe(count_t c) : Counter(c) {}
  control_t control_bits() const override { return NOSTROBE; }
  std::string desc() const override { return nostrobestring; }
  std::shared_ptr<Counter> clone() const override { return std::make_shared<NoStrobe>(*this); }
};

const std::string bitloadstring = "(load)"s;
const std::string bitsetstring = "(set)"s;
const std::string bitclearstring = "(clear)"s;
const std::string bitflipstring = "(flip)"s;
const std::string bitnotstring = "(not)"s;
const std::string bitandstring = "(and)"s;
const std::string bitorstring = "(or)"s;
const std::string bitxorstring = "(xor)"s;
const std::string bitxnorstring = "(xnor)"s;
const std::string bitsllstring = "(sll)"s;
const std::string bitsrlstring = "(srl)"s;

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
  virtual value_t result(const value_t v_prev) const { return v; }
  virtual control_t mode_bits() const { return 0; }
  virtual std::string desc() const { return ""; }
  virtual std::shared_ptr<Value> clone() const { return std::make_shared<Value>(*this); } // deep-copy semantics, because copy is cheap
};

// wrapper class for storing a bit setting pattern
class BitLoad : public Value
{
public:
  BitLoad(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v; }
  control_t mode_bits() const override { return BITLOAD; }
  std::string desc() const override { return bitloadstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitLoad>(*this); }
};

// wrapper class for storing a bit setting pattern
class BitSet : public Value
{
public:
  BitSet(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v_prev | v; }
  control_t mode_bits() const override { return BITSET; }
  std::string desc() const override { return bitsetstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitSet>(*this); }
};

// wrapper class for storing a bit clearing pattern
class BitClear : public Value
{
public:
  BitClear(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v_prev & ~v; }
  control_t mode_bits() const override { return BITCLEAR; }
  std::string desc() const override { return bitclearstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitClear>(*this); }
};

// wrapper class for string a bit flipping pattern
class BitFlip : public Value
{
public:
  BitFlip(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v_prev ^ v; }
  control_t mode_bits() const override { return BITFLIP; }
  std::string desc() const override { return bitflipstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitFlip>(*this); }
};

class BitNot : public Value
{
public:
  BitNot(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return ~v_prev; }
  control_t mode_bits() const override { return BITNOT; }
  std::string desc() const override { return bitnotstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitNot>(*this); }
};

class BitAnd : public Value
{
public:
  BitAnd(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v & v_prev; }
  control_t mode_bits() const override { return BITAND; }
  std::string desc() const override { return bitandstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitAnd>(*this); }
};

class BitOr : public Value
{
public:
  BitOr(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v | v_prev; }
  control_t mode_bits() const override { return BITOR; }
  std::string desc() const override { return bitorstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitOr>(*this); }
};

class BitXor : public Value
{
public:
  BitXor(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return v ^ v_prev; }
  control_t mode_bits() const override { return BITXOR; }
  std::string desc() const override { return bitxorstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitXor>(*this); }
};

class BitXnor : public Value
{
public:
  BitXnor(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return ~(v ^ v_prev); }
  control_t mode_bits() const override { return BITXNOR; }
  std::string desc() const override { return bitxnorstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitXnor>(*this); }
};

class BitSll : public Value
{
public:
  BitSll(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return sll(v_prev, v); }
  control_t mode_bits() const override { return BITSLL; }
  std::string desc() const override { return bitsllstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitSll>(*this); }
};

class BitSrl : public Value
{
public:
  BitSrl(value_t _v) : Value(_v) {}
  value_t result(const value_t v_prev) const override { return srl(v_prev, v); }
  control_t mode_bits() const override { return BITSRL; }
  std::string desc() const override { return bitsrlstring; }
  std::shared_ptr<Value> clone() const override { return std::make_shared<BitSrl>(*this); }
};

class TriggerCondition : public Value
{
private:
  bool final;
public:
  TriggerCondition(trigger_t pattern, trigger_t mask, bool _final) : Value(((value_t)mask << WIDTH_TRIGGER) + (value_t)pattern), final(_final) {}
  control_t mode_bits() const override { return TRIGGER | (final ? TRIGGERFINAL : 0); }
  std::string desc() const override { return triggerstring + (final ? finalstring : ""); }
  std::string value_str() const override {
    std::stringstream ss;
    trigger_t pattern = v & TRIGGER_MASK;
    trigger_t mask = (v >> WIDTH_TRIGGER) & TRIGGER_MASK;
    ss << "0x" << std::hex << int(v) << " "
        << "trig=0x" << std::hex << int(pattern) << " {" << std::bitset<WIDTH_TRIGGER>(int(pattern)) << "} "
        << "mask=0x" << std::hex << int(mask)    << " {" << std::bitset<WIDTH_TRIGGER>(int(mask))    << "}";
    return ss.str();
  }
  std::shared_ptr<Value> clone() const override { return std::make_shared<TriggerCondition>(*this); }
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
  std::shared_ptr<Counter> cc;
  std::shared_ptr<Value> vv;

public:
  // General constructor, called internally by other constructors, less appropriate for general use
  el(el_type _t, const Counter &_cc, const Value &_vv, control_t _y = 0) : t(_t), cc(_cc.clone()), vv(_vv.clone()) {
    y = cc->control_bits() | vv->mode_bits() | _y;
  }
  // Sequence terminator
  el(value_t _v = default_final_value) : el(el_type::final, Counter(1), Value(_v), TERMINATE) {}
  // Regular element
  el(count_t _c, value_t _v) : el(el_type::regular, Counter(_c), BitLoad(_v)) {};
  el(const Counter &_cc, value_t &_v) : el(el_type::regular, _cc, BitLoad(_v)) {};
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
  count_t count() const { return cc->count(); }
  value_t value() const { return vv->value(); }

  // Mark an element for storage in fast memory register i
  el& store(unsigned int i) {
    if (i >= POSITIONS)
      throw std::runtime_error("store() out of bounds");
    y |= STORE + (i << SHIFT_POSITION);
    return *this;
  }

  void set_control(control_t _y) { y = _y; }
  void set_count(const Counter &_cc) { cc = _cc.clone(); }
  void set_value(const Value &_vv) { vv = _vv.clone(); }

  // The resulting data value if the previous value was v_prev and the value was updated according to the contained Value object
  value_t updated_value(const value_t v_prev) const { return vv->result(v_prev); }

  bool is_regular() const { return t == el_type::regular; }
  bool is_trigger() const { return t == el_type::trigger; }
  bool is_final() const { return t == el_type::final; }

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
    switch (t) {
    case el_type::regular:
      ss << vv->value_str() << " " << cc->count_str() << " " << cc->desc() << " " << vv->desc();
      ss << " control=0x" << std::hex << control() << decode();
      break;
    case el_type::trigger:
      ss << vv->value_str() << " " << vv->desc();
      break;
    case el_type::replay:
      ss << "Replay: repetitions=" << std::dec << count() << " length=" << std::dec << value();
      break;
    case el_type::final:
      ss << finalstring;
      break;
    case el_type::retrig:
      ss << retrigstring;
      break;
    case el_type::prng:
      ss << prngstring << " length=" << std::dec << count();
      break;
    }
    return ss.str();
  }

  friend inline std::ostream &operator<<(std::ostream &o, const el &e) {
    o << e.desc();
    return o;
  }

  // We only compare the values, the type of stored objects is ignored.
  friend inline bool operator==(const el &X, const el &Y) {
    return X.y == Y.y && X.cc->count() == Y.cc->count() && X.vv->value()  == Y.vv->value();
  }

  friend inline bool operator!=(const el &X, const el &Y) { return !(X == Y); }
};
