// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Control over the output from multiple streamers

#pragma once

#include <iostream>

#include "fpga.hh"
#include "combiner.hh"
#include "parser.hh"

class combiner_qout : public combiner {
 public:
   combiner_qout(const mm &dev, const std::uintptr_t base) :
     combiner::combiner(dev, base) {}
};

// High-level control of the output
class qout {
 private:
   FPGA &fpga;
   const Verbosity &verb;

 public:
   combiner_qout cq;

   qout(const InputParser &input, const Verbosity &_v, FPGA &_fpga) :
     fpga(_fpga),
     verb(_v),
     cq(fpga.dev_h2f, COMBINER_QOUT_BASE)
     { set(input); }

   void set(const InputParser &input) {
     if (input.exists("-out_sel1")) {
       cq.mode(comb_mode::SEL1);
       if (verb.veryverbose) std::cout << "Output mode SEL1: streamer 1" << std::endl;
     } else if (input.exists("-out_sel2")) {
       cq.mode(comb_mode::SEL2);
       if (verb.veryverbose) std::cout << "Output mode SEL2: streamer 2" << std::endl;
     } else if (input.exists("-out_sel3")) {
       cq.mode(comb_mode::SEL3);
       if (verb.veryverbose) std::cout << "Output mode SEL3: streamer 3" << std::endl;
     } else if (input.exists("-out_sel4")) {
       cq.mode(comb_mode::SEL4);
       if (verb.veryverbose) std::cout << "Output mode SEL4: streamer 4" << std::endl;
     } else if (input.exists("-out_and")) {
       cq.mode(comb_mode::AND);
       if (verb.veryverbose) std::cout << "Output mode AND: conjunction" << std::endl;
     } else if (input.exists("-out_or")) {
       cq.mode(comb_mode::OR);
       if (verb.veryverbose) std::cout << "Output mode OR: inclusive disjunction" << std::endl;
     } else if (input.exists("-out_xor")) {
       cq.mode(comb_mode::XOR);
       if (verb.veryverbose) std::cout << "Output mode XOR: odd-parity (exclusive disjunction)" << std::endl;
     } else if (input.exists("-out_xnor")) {
       cq.mode(comb_mode::XNOR);
       if (verb.veryverbose) std::cout << "Output mode XNOR: even-parity" << std::endl;
     } else if (input.exists("-out_maj")) {
       cq.mode(comb_mode::MAJ);
       if (verb.veryverbose) std::cout << "Output mode MAJ: majority value" << std::endl;
     } else if (input.exists("-out_block8")) {
       cq.mode(comb_mode::BLOCK8);
       if (verb.veryverbose) std::cout << "Output mode BLOCK8: 8 bits per streamer (LSB from streamer 0)" << std::endl;
     } else if (input.exists("-out_block16")) {
       cq.mode(comb_mode::BLOCK16);
       if (verb.veryverbose) std::cout << "Output mode BLOCK16: 16 bits from streamer 0, 16 bits from streamer 1" << std::endl;
     } else if (input.exists("-out_sum12")) {
       cq.mode(comb_mode::SUM12);
       if (verb.veryverbose) std::cout << "Output mode SUM12: algebraic sum of streamers 1 and 2" << std::endl;
     } else if (input.exists("-out_sum1234")) {
       cq.mode(comb_mode::SUM1234);
       if (verb.veryverbose) std::cout << "Output mode SUM1234: algebraic total" << std::endl;
     } else if (input.exists("-out_diff12")) {
       cq.mode(comb_mode::DIFF12);
       if (verb.veryverbose) std::cout << "Output mode DIFF12: difference (2-1)" << std::endl;
     } else {
       // default
       cq.mode(comb_mode::SEL1);
       if (verb.veryverbose) std::cout << "Output mode SEL1 (default): streamer 1" << std::endl;
     }
     if (input.exists("-invert1")) {
       auto v = parse_value(input, "-invert1", "0");
       if (verb.veryverbose) std::cout << "Port 1 inverting: " << binary_digits(v) << std::endl;
       cq.invert(1, v);
     }
     if (input.exists("-mask1")) {
       auto v = parse_value(input, "-mask1", "0");
       if (verb.veryverbose) std::cout << "Port 1 mask: " << binary_digits(v) << std::endl;
       cq.mask(1, v);
     }
     if (input.exists("-force1")) {
       auto v = parse_value(input, "-force1", "0");
       if (verb.veryverbose) std::cout << "Port 1 force: " << binary_digits(v) << std::endl;
       cq.force(1, v);
     }
     if (input.exists("-invert2")) {
       auto v = parse_value(input, "-invert2", "0");
       if (verb.veryverbose) std::cout << "Port 2 inverting: " << binary_digits(v) << std::endl;
       cq.invert(2, v);
     }
     if (input.exists("-mask2")) {
       auto v = parse_value(input, "-mask2", "0");
       if (verb.veryverbose) std::cout << "Port 2 mask: " << binary_digits(v) << std::endl;
       cq.mask(2, v);
     }
     if (input.exists("-force2")) {
       auto v = parse_value(input, "-force2", "0");
       if (verb.veryverbose) std::cout << "Port 2 force: " << binary_digits(v) << std::endl;
       cq.force(2, v);
     }
     if (input.exists("-invert3")) {
       auto v = parse_value(input, "-invert3", "0");
       if (verb.veryverbose) std::cout << "Port 3 inverting: " << binary_digits(v) << std::endl;
       cq.invert(3, v);
     }
     if (input.exists("-mask3")) {
       auto v = parse_value(input, "-mask3", "0");
       if (verb.veryverbose) std::cout << "Port 3 mask: " << binary_digits(v) << std::endl;
       cq.mask(3, v);
     }
     if (input.exists("-force3")) {
       auto v = parse_value(input, "-force3", "0");
       if (verb.veryverbose) std::cout << "Port 3 force: " << binary_digits(v) << std::endl;
       cq.force(3, v);
     }
     if (input.exists("-invert4")) {
       auto v = parse_value(input, "-invert4", "0");
       if (verb.veryverbose) std::cout << "Port 4 inverting: " << binary_digits(v) << std::endl;
       cq.invert(4, v);
     }
     if (input.exists("-mask4")) {
       auto v = parse_value(input, "-mask4", "0");
       if (verb.veryverbose) std::cout << "Port 4 mask: " << binary_digits(v) << std::endl;
       cq.mask(4, v);
     }
     if (input.exists("-force4")) {
       auto v = parse_value(input, "-force4", "0");
       if (verb.veryverbose) std::cout << "Port 4 force: " << binary_digits(v) << std::endl;
       cq.force(4, v);
     }
     if (input.exists("-invert_out")) {
       auto v = parse_value(input, "-invert_out", "0");
       if (verb.veryverbose) std::cout << "Output inverting: " << binary_digits(v) << std::endl;
       cq.invert(0, v);
     }
     if (input.exists("-mask_out")) {
       auto v = parse_value(input, "-mask_out", "0");
       if (verb.veryverbose) std::cout << "Output mask: " << binary_digits(v) << std::endl;
       cq.mask(0, v);
     }
     if (input.exists("-force_out")) {
       auto v = parse_value(input, "-force_out", "0");
       if (verb.veryverbose) std::cout << "Output force: " << binary_digits(v) << std::endl;
       cq.force(0, v);
     }
   }

   auto in1() { return cq.in1(); }
   auto in2() { return cq.in2(); }
   auto in3() { return cq.in3(); }
   auto in4() { return cq.in4(); }
   auto out() { return cq.out(); }
};

value_t combine(comb_mode cm, value_t y1, value_t y2, value_t y3, value_t y4) {
  switch (cm) {
  case comb_mode::SEL1:
    return y1;
  case comb_mode::SEL2:
    return y2;
  case comb_mode::SEL3:
    return y3;
  case comb_mode::SEL4:
    return y4;
  case comb_mode::AND:
    return y1 & y2 & y3 & y4;
  case comb_mode::OR:
    return y1 | y2 | y3 | y4;
  case comb_mode::XOR:
    return y1 ^ y2 ^ y3 ^ y4;
  case comb_mode::XNOR:
    return ~(y1 ^ y2 ^ y3 ^ y4);
  case comb_mode::MAJ:
    return bitwise_majority4(y1, y2, y3, y4);
  case comb_mode::BLOCK8:
    return (y1 & 0xFF) + ((y2 & 0xFF) << 8) + ((y3 & 0xFF) << 16) + ((y4 & 0xFF) << 24);
  case comb_mode::BLOCK16:
    return (y1 & 0xFFFF) + ((y2 & 0xFFFF) << 16);
  case comb_mode::SUM12:
    return y1+y2;
  case comb_mode::SUM1234:
    return y1+y2+y3+y4;
  case comb_mode::DIFF12:
    return y1-y2;
  }
  never_reached();
  return 0;
}
