#pragma once

// Host-side wrapper for the integrated counter/measurement subsystem.
//
// This file mirrors the compact selector-based programming model implemented by
// `ip/counter/counter_if.sv`: software selects an instrument, chooses the high/low word
// and instrument-local address, optionally selects channels, then reads results through a
// shared register bank. Architectural overview lives in `docs/docs/counter.md` and
// `ip/counter/README.md`.

#include <algorithm>
#include <functional>
#include <cstdint>
#include <bitset>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "tidbit.hh"
#include "fpga.hh"
#include "colors.hh"
#include "sequence.hh"

#define COUNTER_AC

using readfnc = std::function<uint32_t(uint32_t, uint32_t)>;

class basic_counter {
 private:
   readfnc rd;
 public:
   basic_counter(readfnc _rd) { rd = std::move(_rd); }

   uint64_t read(uint32_t addr) {
     return to64(rd(0, addr), rd(1, addr));
   }

   uint64_t total() {
     return read(0);
   }

   uint64_t l() {
     return read(2);
   }

   uint64_t h() {
     return read(3);
   }

   uint64_t lh() { // low-to-high (assertions)
     return read(4);
   }

   uint64_t hl() { // high-to-low (deassertions)
     return read(5);
   }

   std::string str() {
     std::stringstream ss;
     ss << "total=" << with_underscores(total());
     ss << " l=" << with_underscores(l());
     ss << " h=" << with_underscores(h());
     ss << " lh=" << with_underscores(lh());
     ss << " hl=" << with_underscores(hl());
     return ss.str();
   }
};

class runs_counter {
 private:
   readfnc rd;
 public:
   runs_counter(readfnc _rd) { rd = std::move(_rd); }

   uint64_t read(uint32_t addr) {
     return to64(rd(0, addr), rd(1, addr));
   }

   uint64_t ctr_run() {
     return read(0);
   }

   uint64_t nr_run_l() {
     return read(2);
   }

   uint64_t nr_run_h() {
     return read(3);
   }

   uint64_t sum_run_l() {
     return read(4);
   }

   uint64_t sum_run_h() {
     return read(5);
   }

#ifdef DO_SUM2
   uint64_t sum2_run_l() {
     return read(12);
   }

   uint64_t sum2_run_h() {
     return read(13);
   }
#endif

   uint64_t max_run_l() {
     return read(6);
   }

   uint64_t max_run_h() {
     return read(7);
   }

   uint64_t nr_glitch_l() {
     return read(8);
   }

   uint64_t nr_glitch_h() {
     return read(9);
   }

   std::string str() {
     std::stringstream ss;
     auto nr_l = nr_run_l();
     auto nr_h = nr_run_h();
     ss << "runs=" << with_underscores(ctr_run());
     ss << " l=" << with_underscores(nr_l);
     ss << " h=" << with_underscores(nr_h);
     ss << " max_l=" << with_underscores(max_run_l());
     ss << " max_h=" << with_underscores(max_run_h());
     ss << " nr_glitch_l=" << with_underscores(nr_glitch_l());
     ss << " nr_glitch_h=" << with_underscores(nr_glitch_h());
     ss << " sum_l=" << with_underscores(sum_run_l());
#ifdef DO_SUM2
     ss << " sum2_l=" << with_underscores(sum2_run_l());
#endif
     ss << " sum_h=" << with_underscores(sum_run_h());
#ifdef DO_SUM2
     ss << " sum2_h=" << with_underscores(sum2_run_h());
#endif
     auto avg_l = double(sum_run_l())/nr_l;
     auto avg_h = double(sum_run_h())/nr_h;
     ss << " avg_l=" << avg_l;
     ss << " avg_h=" << avg_h;
#ifdef DO_SUM2
     auto var_l = double(sum2_run_l())/nr_l-std::pow(avg_l,2);
     auto dev_l = std::sqrt(var_l);
     auto var_h = double(sum2_run_h())/nr_h-std::pow(avg_h,2);
     auto dev_h = std::sqrt(var_h);
     ss << " dev_l=" << dev_l;
     ss << " dev_h=" << dev_h;
#endif
     return ss.str();
   }
};

class packet_stats {
 private:
   readfnc rd;
 public:
   packet_stats(readfnc _rd) { rd = std::move(_rd); }

   uint64_t read(uint32_t addr) {
     return to64(rd(0, addr), rd(1, addr));
   }

   uint64_t total() {
     return read(0);
   }

   uint64_t valid() {
     return read(1);
   }

   uint64_t idle() {
     return read(2);
   }

   uint64_t pkt_begin() {
     return read(3);
   }

   uint64_t pkt_end() {
     return read(4);
   }

   uint64_t pkt_len_sum() {
     return read(5);
   }

   uint64_t pkt_len_sum2() {
     return read(6);
   }

   std::string str() {
     std::stringstream ss;
     ss << "ticks=" << with_underscores(total());
     ss << " valid=" << with_underscores(valid());
     ss << " idle=" << with_underscores(idle());
     auto bg = pkt_begin();
     auto en = pkt_end();
     ss << " pkt_begin=" << with_underscores(bg);
     ss << " pkt_end=" << with_underscores(en);
     auto sum = pkt_len_sum();
     auto sum2 = pkt_len_sum2();
     ss << " pkt_len_sum=" << with_underscores(sum);
     ss << " pkt_len_sum2=" << with_underscores(sum2);
     auto nr = std::max(bg, en);
     auto avg = double(sum)/nr;
     auto var = double(sum2)/nr - pow(avg, 2);
     auto dev = sqrt(var);
     ss << " avg_len=" << avg << " dev=" << dev;
     return ss.str();
   }
};

class seq_counter {
 private:
   size_t nr;
   readfnc rd;
 public:
   seq_counter(size_t _nr, readfnc _rd) { nr = _nr; rd = std::move(_rd); }

   uint64_t read(uint32_t addr) {
     return to64(rd(0, addr), rd(1, addr));
   }

   uint64_t ctr(uint32_t seq) {
     return read(seq);
   }

   std::string str() {
     std::stringstream ss;
     for (size_t i = 0; i < nr; i++)
       ss << "[" << std::bitset<4>(i) << "] " << with_underscores(ctr(i)) << "\n"; // hardcoded 4
     return ss.str();
   }
};

class autocorrelation {
 private:
   size_t nr;
   readfnc rd;
 public:
   autocorrelation(size_t _nr, readfnc _rd) { nr = _nr; rd = std::move(_rd); }

   uint64_t read(uint32_t addr) {
     return to64(rd(0, addr), rd(1, addr));
   }

   uint64_t ctr(uint32_t seq) {
     return read(seq);
   }

   std::string str() {
     std::stringstream ss;
     for (size_t i = 0; i < nr; i++)
       ss << "[" << i << "] " << with_underscores(ctr(i)) << "\n";
     return ss.str();
   }
};

class crosscorrelation {
 private:
   size_t nr;
   readfnc rd;
 public:
   crosscorrelation(size_t _nr, readfnc _rd) { nr = _nr; rd = std::move(_rd); }

   uint64_t read(uint32_t addr) {
     return to64(rd(0, addr), rd(1, addr));
   }

   uint64_t ctr(uint32_t seq) {
     return read(seq);
   }

   std::string str() {
     std::stringstream ss;
     for (size_t i = 0; i < nr; i++)
       ss << "[" << i << "] " << with_underscores(ctr(i)) << "\n";
     return ss.str();
   }
};

class counter {
 public:
   FPGA &fpga;
   mm &dev;
   loc linstr, // instrument number
     lpart,    // word part (high/low)
     laddr,    // port address
     lsel0,    // multiplexer sel0
     lsel1,    // multiplexer sel1
     lsel2,    // multiplexer sel2
     lctrl;    // control (latch_all, reset_all)
   loc lresult,
     loverflow_bc,
     loverflow_pc,
     ltcready;
   int sel0, sel1, sel2;
   basic_counter bc;
   runs_counter rc;
   packet_stats ps;
   seq_counter sc;
   autocorrelation ac;
   crosscorrelation cc;

   counter(const InputParser &input, FPGA &_fpga, const std::uintptr_t base = COUNTER_Q_BASE) :
     fpga(_fpga),
     dev(fpga.dev_h2f),
     linstr(dev.get_loc(base, 1*4)),
     lpart(dev.get_loc(base, 2*4)),
     laddr(dev.get_loc(base, 3*4)),
     lsel0(dev.get_loc(base, 4*4)),
     lsel1(dev.get_loc(base, 5*4)),
     lsel2(dev.get_loc(base, 6*4)),
     lctrl(dev.get_loc(base, 7*4)),
     lresult(dev.get_loc(base, 0*4)),
     loverflow_bc(dev.get_loc(base, 1*4)),
     loverflow_pc(dev.get_loc(base, 2*4)),
     ltcready(dev.get_loc(base, 3*4)),
     bc(readfnc([&](uint32_t part, uint32_t addr) { return read(1, part, addr); })),
     rc(readfnc([&](uint32_t part, uint32_t addr) { return read(2, part, addr); })),
     ps(readfnc([&](uint32_t part, uint32_t addr) { return read(5, part, addr); })),
     sc(16, readfnc([&](uint32_t part, uint32_t addr) { return read(3, part, addr); })),
     ac(16, readfnc([&](uint32_t part, uint32_t addr) { return read(6, part, addr); })),
     cc(16, readfnc([&](uint32_t part, uint32_t addr) { return read(7, part, addr); }))
   {
     sel0 = 0;
     sel1 = 0;
     sel2 = 1;
   }

   // Read one logical measurement word from the selector-based counter backplane.
   uint32_t read(uint32_t instr, uint32_t part, uint32_t addr) {
     linstr.write(instr);
     lpart.write(part);
     laddr.write(addr);
     return lresult.read();
   }

   // Reset is synchronized into the sampled-data domain, so the pulse must be long
   // enough relative to the active streamer clock.
   void reset_all() {
     lctrl.write(1);
     fpga.wait_for_N_streamer_clk_periods(2);  // reset is synchronous, thus this should be longer than the period of streaming clock
     lctrl.write(0);
   }

   // Latch all instruments so the following read sequence observes a stable snapshot.
   void latch_all() {
     lctrl.write(2);
     usleep(10);
     lctrl.write(0);
   }

   void report() {
     std::cout << "Basic statistics, ch=" << sel0 << std::endl;
     std::cout << bc.str() << std::endl;
     std::cout << "Runs statistics, ch=" << sel0 << std::endl;
     std::cout << rc.str() << std::endl;
     std::cout << "Packet statistics, ch=" << sel0 << std::endl;
     std::cout << ps.str() << std::endl;
     std::cout << "Short-sequence statistics, ch=" << sel0 << std::endl;
     std::cout << sc.str();
#ifdef COUNTER_AC
     std::cout << "Autocorrelations, ch=" << sel0 << std::endl;
     std::cout << ac.str();
#endif
#ifdef COUNTER_CC
     std::cout << "Crosscorrelations, chs=" << sel1 << "," << sel2 << std::endl;
     std::cout << cc.str();
#endif
   }

   void short_report() {
     std::cout << "Basic statistics, ch=" << sel0 << std::endl;
     std::cout << bc.str() << std::endl;
     std::cout << "Runs statistics, ch=" << sel0 << std::endl;
     std::cout << rc.str() << std::endl;
   }
};

inline int compare(const std::string what, const uint64_t value, const uint64_t expected) {
  if (value == expected)
    return 0;
  std::cerr << red << what << " mismatch: value=" << value << " expected=" << expected << rst << std::endl;
  return 1;
}

#define test_equal(a, b) compare(std::string(#a), a, b)

inline auto counter_seq1()
{
  // Deterministic reference sequence used by the built-in self-check path.
  Sequence seq;
  seq.push_back(el(1, 0));
  seq.push_back(el(1, 0xff));
  seq.push_back(el(1, 0));
  seq.push_back(el(1, 0xff));
  seq.push_back(el(1, 0));
  seq.push_back(el(2, 0xff));
  seq.push_back(el(2, 0));
  seq.push_back(el());
  return seq;
}

inline auto counter_seq2(const InputParser &input)
{
  // Longer pseudo-random activity source used for exploratory measurements and stress.
  const auto c = parse_count(input, "-c", "1000");
  Sequence seq;
  if (c > 0)
    seq.push_back(el(PseudoRandom{}, c));
  seq.push_back(el());
  return seq;
}

inline int counter_test1(counter &ctr)
{
  int rc = 0;
  rc |= test_equal(ctr.bc.total(), 9);
  rc |= test_equal(ctr.bc.l(), 5);
  rc |= test_equal(ctr.bc.h(), 4);
  rc |= test_equal(ctr.bc.lh(), 3);
  rc |= test_equal(ctr.bc.hl(), 3);
  rc |= test_equal(ctr.rc.ctr_run(), 7);
  rc |= test_equal(ctr.rc.nr_run_l(), 4);
  rc |= test_equal(ctr.rc.nr_run_h(), 3);
  rc |= test_equal(ctr.rc.sum_run_l(), 5);
  rc |= test_equal(ctr.rc.sum_run_h(), 4);
#ifdef DO_SUM2
  rc |= test_equal(ctr.rc.sum2_run_l(), 7);
  rc |= test_equal(ctr.rc.sum2_run_h(), 6);
#endif
  rc |= test_equal(ctr.rc.max_run_l(), 2);
  rc |= test_equal(ctr.rc.max_run_h(), 2);
  rc |= test_equal(ctr.rc.nr_glitch_l(), 3);
  rc |= test_equal(ctr.rc.nr_glitch_h(), 2);
  rc |= test_equal(ctr.ps.valid(), 9);
  rc |= test_equal(ctr.ps.pkt_begin(), 1);
  rc |= test_equal(ctr.ps.pkt_end(), 1);
  rc |= test_equal(ctr.ps.pkt_len_sum(), 9);
  rc |= test_equal(ctr.ps.pkt_len_sum2(), 81);
  rc |= test_equal(ctr.sc.ctr(0b0000), 0);
  rc |= test_equal(ctr.sc.ctr(0b0101), 1);
  rc |= test_equal(ctr.sc.ctr(0b0110), 1);
#ifdef COUNTER_AC
  rc |= test_equal(ctr.ac.ctr(0), 9);
  rc |= test_equal(ctr.ac.ctr(1), 2);
  rc |= test_equal(ctr.ac.ctr(2), 4);
  rc |= test_equal(ctr.ac.ctr(3), 2);
//  rc |= test_equal(ctr.ac.ctr(4), 3);
//  rc |= test_equal(ctr.ac.ctr(5), 2);
//  rc |= test_equal(ctr.ac.ctr(6), 1);
//  rc |= test_equal(ctr.ac.ctr(7), 1);
//  rc |= test_equal(ctr.ac.ctr(8), 1);
//  rc |= test_equal(ctr.ac.ctr(9), 0);
#endif
#ifdef COUNTER_CC
  rc |= test_equal(ctr.cc.ctr(0), 9);
  rc |= test_equal(ctr.cc.ctr(1), 2);
  rc |= test_equal(ctr.cc.ctr(2), 4);
  rc |= test_equal(ctr.cc.ctr(3), 2);
  rc |= test_equal(ctr.cc.ctr(4), 3);
  rc |= test_equal(ctr.cc.ctr(5), 2);
  rc |= test_equal(ctr.cc.ctr(6), 1);
  rc |= test_equal(ctr.cc.ctr(7), 1);
  rc |= test_equal(ctr.cc.ctr(8), 1);
  rc |= test_equal(ctr.cc.ctr(9), 0);
#endif
  if (rc == 0)
    std::cout << green << "PASSED." << rst << std::endl;
  return rc;
}
