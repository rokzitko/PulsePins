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

#include <fstream>
#include <iostream>
#include <string>
#include <deque>

#include "elements.hh"
#include "vcd_parser.hh"

// Thin extension of `std::deque<el>` with helpers that reflect the semantics of a
// pulse sequence rather than just container operations.
class Sequence : public std::deque<el> {
 public:
   using Base = std::deque<el>;
   using Base::Base;

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
    Sequence convert_to_BitLoad() {
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
    Sequence merge() {
     Sequence s = *this; // make a copy
     merge_adjacent<el>(s,
                        [](const el &x, const el &y){ return x.is_regular() && y.is_regular() && x.control() == y.control() && x.value() == y.value(); },
                        [](const el &x, const el &y){ return el(Counter(x.count() + y.count()), Value(x.value())); });
     return s;
   }

    // Build a sequence from a VCD signal trace. Consecutive samples become run-length
    // encoded elements targeting `target_name`.
    void load_VCD(const std::string filename, const std::string target_name = "outs", const uint32_t scale_factor = 10) {
     std::ifstream F(filename);
     auto l = parseVcdUpdates(F, target_name, scale_factor);
     for (size_t i = 0; i < l.size()-1; i++) {
       Counter c = l[i+1].count-l[i].count;
       Value v = l[i].value;
       this->push_back(el(c, v));
     }
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

inline std::pair<Sequence, bool> parse_sequence_from_stream(std::istream &f)
{
  // Text grammar accepted here:
  //   d <count> <value>           regular data element
  //   t <pattern> <mask>          trigger-condition element
  //   f                           request forced trigger instead of arm-and-wait
  // The returned boolean carries that forced-trigger request alongside the sequence.
  Sequence elements;
  bool force_trigger = false;
  try {
    while (f) {
      std::string token;
      f >> token;
      if (token == "d") {
        std::string sc, sv;
        f >> sc >> sv;
        auto c = parse_count_t(sc);
        auto v = parse_value_t(sv);
        elements.push_back(el(c, v));
      }
      if (token == "t") {
        std::string sp, sm;
        f >> sp >> sm;
        auto p = parse_trigger_t(sp);
        auto m = parse_trigger_t(sm);
        elements.push_back(el(p, m, true));
      }
      if (token == "f") {
        force_trigger = true;
      }
    }
  } catch (const std::exception& e) {
    std::cout << "Caught exception in parse_sequence_from_stream(): " << e.what() << std::endl;
    throw;
  }
  return {elements, force_trigger};
}
