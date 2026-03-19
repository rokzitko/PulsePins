// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Utilities for generating sequences of interest, e.g. for testing purposes

#pragma once

#include <iostream>

#include "elements.hh"
#include "sequence.hh"
#include "streamer.hh"

inline value_t random_value() { return random_u32(); }
inline count_t random_count() { return random_u32(); }

inline Sequence prepare_counter_sequence(const size_t v,
                                         const size_t c,
                                         const bool verbose = false)
{
  if (verbose) std::cout << "v=" << std::dec << v << " c=" << c << std::endl;
  Sequence elements;
  for (size_t i = 0; i < v; i++)
    elements.push_back(el(c, i));
  return elements;
}

// Create a sequence of 'nr' blocks of maximum length 'max_block_len'
inline Sequence prepare_random_test_sequence(const unsigned long nr = 10,
                                             const unsigned long max_block_len = 100000,
                                             const bool rnd = false,
                                             const bool verbose = false)
{
  if (verbose) std::cout << "nr=" << nr << " max_block_len=" << max_block_len << (rnd ? " with randomized values" : "") << std::endl;
  Sequence elements;
  uint64_t len = 0; // total number of elements
  value_t value = 0;
  value_t previous_value = 0; // for randomized tests to prevent collisions
  for (unsigned long i = 0; i < nr; i++) {
    if (rnd) {
      do {
        value = random_value();
      } while (value == previous_value);
    } else {
      value = i;
    }
    el e{Strobe((random_count() % max_block_len) + 1), BitLoad(value)}; // blocks of random length
    elements.push_back(e);
    len += e.count();
    previous_value = value;
  }
  if (verbose) std::cout << "len=" << len << std::endl;
  return elements;
}

// Prepare a random sequence where the update types are also random (update, set, clear, flip, not, and, or,...)
inline Sequence prepare_general_random_test_sequence(const unsigned long nr = 10,
                                                     const unsigned long max_block_len = 100000,
                                                     const bool verbose = false)
{
  if (verbose) std::cout << "nr=" << nr << " max_block_len=" << max_block_len << std::endl;
  Sequence elements;
  uint64_t len = 0; // total number of elements
  value_t value = 0;
  value_t previous_value = 0; // prevent collisions
  for (unsigned long i = 0; i < nr; i++) {
    const auto rnd_len = (random_count() % max_block_len) + 1;
    el e;
    do {
      const auto rnd_value = random_value();
      auto op = random_u32() % 11;
      if (i == 0) // the first element must always be a BitLoad() update
        op = 0;
      switch (op) {
      case 0:
        e = el{Strobe(rnd_len), BitLoad(rnd_value)};
        break;
      case 1:
        e = el{Strobe(rnd_len), BitSet(rnd_value)};
        break;
      case 2:
        e = el{Strobe(rnd_len), BitClear(rnd_value)};
        break;
      case 3:
        e = el{Strobe(rnd_len), BitFlip(rnd_value)};
        break;
      case 4:
        e = el{Strobe(rnd_len), BitNot(0)};
        break;
      case 5:
        e = el{Strobe(rnd_len), BitAnd(rnd_value)};
        break;
      case 6:
        e = el{Strobe(rnd_len), BitOr(rnd_value)};
        break;
      case 7:
        e = el{Strobe(rnd_len), BitXor(rnd_value)};
        break;
      case 8:
        e = el{Strobe(rnd_len), BitXnor(rnd_value)};
        break;
      case 9:
        e = el{Strobe(rnd_len), BitSll(rnd_value % 32)};
        break;
      case 10:
        e = el{Strobe(rnd_len), BitSrl(rnd_value % 32)};
        break;
      }
      value = e.updated_value(previous_value);
    } while (value == previous_value);
    elements.push_back(e);
    len += e.count();
    previous_value = value;
  }
  if (verbose) std::cout << "len=" << len << std::endl;
  return elements;
}

// Create a sequence of 'nr' pulses spaced maximally by 'max_block_len'
inline Sequence prepare_random_pulses(unsigned long nr = 10, unsigned long max_block_len = 100000, bool verbose = false)
{
   if (verbose) std::cout << "nr=" << nr << " max_block_len=" << max_block_len << std::endl;
   Sequence elements;
   for (unsigned long i = 0; i < nr; i++) {
      el e_wait{Strobe((random_u32() % max_block_len) + 1), BitLoad(0)}; // blocks of random length
      elements.push_back(e_wait);
      el e_pulse{Strobe(1), BitLoad(1)};
      elements.push_back(e_pulse);
   }
   return elements;
}

Sequence prepare_equally_spaced_pulses(unsigned long nr = 10, unsigned long block_len = 100000, bool verbose = false)
{
   if (verbose) std::cout << "nr=" << nr << " block_len=" << block_len << std::endl;
   Sequence elements;
   for (unsigned long i = 0; i < nr; i++) {
      el e_wait{Strobe(block_len-1), BitLoad(0)};
      elements.push_back(e_wait);
      el e_pulse{Strobe(1), BitLoad(1)};
      elements.push_back(e_pulse);
   }
   return elements;
}
