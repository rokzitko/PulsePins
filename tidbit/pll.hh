// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Phase-locked-loop

#pragma once

#include <iostream>
#include <cmath>
#include <sstream>

enum class pll_mode { waitrequest, polling };

// Documentation:
// - AN661
// - Altera Phase-Locked Loop (Altera PLL) IP Core User Guide, 2017.06.16
// - https://www.youtube.com/watch?v=-t2bdjkoZzY
// - SWRA029: Fractional/integer PLL basics (Texas Instruments)

// f_out = M/(N*C) * f_ref

class pll {
 private:
   loc mode, status, start, N, M, C, C0, C1, K, BW, CP;
   bool verbose = false;
   std::ostream &F = std::cout;
   int X = 32;
 public:
   pll(const mm &dev, const std::uintptr_t base, std::string name = "pll"s) :
     mode(dev.get_loc(base,   0b000000*4), name + "/mode"), // rw: 0 = waitrequest, 1 = polling mode
     status(dev.get_loc(base, 0b000001*4), name + "/status"), // read-only, 0 = busy, 1 = ready
     start(dev.get_loc(base,  0b000010*4), name + "/start"), // write-only, write 0 or 1 to start reconfiguration
     N(dev.get_loc(base,      0b000011*4), name + "/N"), // bypass-enable & odd/even division bits are read-only
     M(dev.get_loc(base,      0b000100*4), name + "/M"),
     C(dev.get_loc(base,      0b000101*4), name + "/C"),
     C0(dev.get_loc(base,     0b001010*4), name + "/C0"), // counters
     C1(dev.get_loc(base,     0b001011*4), name + "/C1"),
     // dynamic_phase_shift
     K(dev.get_loc(base,      0b000111*4), name + "/K"), // write-only
     BW(dev.get_loc(base,     0b001000*4), name + "/BW"),
     CP(dev.get_loc(base,     0b001001*4), name + "/CP")
   {}
   // Waitrequest: further writes on hold while PLL busy.
   void setmode(const pll_mode m = pll_mode::polling) {
     if (m == pll_mode::waitrequest)
       mode.write(0);
     if (m == pll_mode::polling)
       mode.write(1);
   }
   // Register value to a string with low and high count parts
   std::string split(const uint32_t val) {
     uint8_t low_count = val & 0xFF;
     uint8_t high_count = (val & 0xFF00)>>8;
     return "[" + std::to_string(low_count) + "," + std::to_string(high_count) + "]";
   }
   // Register value to a sum of low and high count parts
   int convert(const uint32_t val) {
     uint8_t low_count = val & 0xFF;
     uint8_t high_count = (val & 0xFF00)>>8;
     return low_count+high_count;
   }
   // Report the PLL configuration; show all registers of relevance
   void report() {
     const auto valm = mode.read();
     F << "mode=" << std::hex << valm << (valm == 0 ? "[waitrequest]"s : "[polling]"s);
     if (valm == 1) { // if polling
       const auto vals = status.read();
       F << " status=" << std::hex << vals << (vals == 0 ? "[busy]"s : "[ready]"s);
     }
     const auto valN = N.read();
     F << " N=0x" << std::hex << valN << std::dec << split(valN);
     const auto valM = M.read();
     F << " M=0x" << std::hex << valM << std::dec << split(valM);
     const auto valC0 = C0.read();
     F << " C0=0x" << std::hex << valC0 << std::dec << split(valC0);
     const auto valC1 = C1.read();
     F << " C1=0x" << std::hex << valC1 << std::dec << split(valC1);
     F << " bandwidth=" << std::hex << BW.read();
     F << " charge pump=" << std::hex << CP.read() << std::endl;
   }
   // ex=true: execute the reconfiguration; ex=false: part of a longer sequences of
   void set_N(const int val, const bool ex) { // val = Total_div
     if (ex) setmode(pll_mode::waitrequest);
     assert(val >= 1 && val < 512);
     const uint8_t half = val/2;
     const uint8_t rest = val-half;
     const uint32_t x = half*256 + rest;
     if (verbose) F << "Setting N=" << std::dec << val << " = 0x" << std::hex << x << std::endl;
     N.write(x);
     if (ex) start.write(1); // start dynamic reconfiguration
   }
   void set_M(const int val, const bool ex) { // val = Total_div
     if (ex) setmode(pll_mode::waitrequest);
     assert(val >= 1 && val < 512);
     const uint8_t half = val/2;
     const uint8_t rest = val-half;
     const uint32_t x = half*256 + rest;
     if (verbose) F << "Setting M=" << std::dec << val << " = 0x" << std::hex << x << std::endl;
     M.write(x);
     if (ex) start.write(1); // start dynamic reconfiguration
   }
   void set_C(const int val, const bool ex, int ndx = 0) { // val = Total_div
     if (ex) setmode(pll_mode::waitrequest);
     assert(val >= 1 && val < 512);
     assert(ndx >= 0 && ndx <= 17);
     const uint8_t half = val/2;
     const uint8_t rest = val-half;
     uint32_t x = half*256 + rest;
     bool bypass_enable = false;
     if (bypass_enable) x += 1<<16;
     bool odd_division = false;
     if (odd_division) x += 1<<17;
     x += (ndx << 18);
     if (verbose) F << "Setting C=" << std::dec << val << " = 0x" << std::hex << x << std::endl;
     C.write(x);
     if (ex) start.write(1); // start dynamic reconfiguration
   }
   void set_K(const uint32_t val, const bool ex) {
     if (ex) setmode(pll_mode::waitrequest);
     const float Mfrac = float(val)/pow(2,X);
     if (verbose) F << "Setting K=" << val << " Mfrac=" << Mfrac << std::endl;
     K.write(val);
     if (ex) start.write(1);
   }
   // Configure N,M,C from a comma-separeted list of values
   void set_from_string(const std::string s) {
     if (s == "") return;
     std::istringstream iss(s);
     int N, M, C;
     char comma;
     if (iss >> N >> comma >> M >> comma >> C) {
       setmode(pll_mode::waitrequest);
       set_N(N, false);
       set_M(M, false);
       set_C(C, false);
       start.write(1);
     } else {
       throw std::runtime_error("error parsing PLL config string");
     }
   }
   // Version for two output clocks (and two different C settings)
   void set_from_string_2clk(const std::string s) {
     if (s == "") return;
     std::istringstream iss(s);
     int N, M, C0, C1;
     char comma;
     if (iss >> N >> comma >> M >> comma >> C0 >> comma >> C1) {
       setmode(pll_mode::waitrequest);
       set_N(N, false);
       set_M(M, false);
       set_C(C0, false, 0);
       set_C(C1, false, 1);
       start.write(1);
     } else {
       throw std::runtime_error("error parsing PLL config string");
     }
   }
   void set_charge_pump(const int v, const bool ex = true) {
     if (ex) setmode(pll_mode::waitrequest);
     assert(v >= 0 && v <= 7);
     CP.write(v);
     if (ex) start.write(1);
   }
   void set_bandwidth(const int v, const bool ex = true) {
     if (ex) setmode(pll_mode::waitrequest);
     assert(v >= 0 && v <= 15);
     BW.write(v);
     if (ex) start.write(1);
   }
   // Current (requested) PLL frequency
   double get_freq(const int ch = 0, const double input_clk = 50*1000*1000.0) {
     assert(0 <= ch && ch <= 1);
     const int n = convert(N.read());
     const int m = convert(M.read());
     const int c = convert(ch == 0 ? C0.read() : C1.read());
     return input_clk * m/(n*c);
   }
};
