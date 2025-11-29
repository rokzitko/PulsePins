// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// High-level interface for triggering control

#pragma once

#include <iostream>
#include <bitset>

#include "fpga.hh"
#include "combiner.hh"
#include "parser.hh"

class trigger {
 private:
   const FPGA &fpga;

 public:
   combiner_trig ct;

   trigger(const InputParser &input, const FPGA &_fpga) :
     fpga(_fpga),
     ct(fpga.dev_h2f, COMBINER_TRIG_BASE) { set(input); }

   // Set the triggering from command line switches
   // Default: internal triggering
   void set(const InputParser &input) {
     if (fpga.v.veryverbose) std::cout << "## Setting up the trigger combiner." << std::endl;
     if (input.exists("-trig_int")) {
       if (fpga.v.veryverbose) std::cout << "Trigger: internal" << std::endl;
       ct.mode(trig_mode::INT);
     }
     if (input.exists("-trig_ext")) {
       if (fpga.v.veryverbose) std::cout << "Trigger: external" << std::endl;
       ct.mode(trig_mode::EXT);
     }
     if (input.exists("-trig_misc")) {
       if (fpga.v.veryverbose) std::cout << "Trigger: misc (pushbuttons + 1PPS)" << std::endl;
       ct.mode(trig_mode::MISC);
     }
     if (input.exists("-trig_any")) {
       if (fpga.v.veryverbose) std::cout << "Trigger: any of" << std::endl;
       ct.mode(trig_mode::OR);
     }
     if (input.exists("-trig_all")) {
       if (fpga.v.veryverbose) std::cout << "Trigger: all of" << std::endl;
       ct.mode(trig_mode::AND);
     }
     if (input.exists("-trig_std")) {
       if (fpga.v.veryverbose) std::cout << "Trigger: standard (any of)" << std::endl;
       ct.invert_ext(~0); // invert all external signals (they are pulled up!)
       ct.mode(trig_mode::OR);
     }
     if (input.exists("-invert_trig_result")) {
       auto v = parse_uint32(input, "-invert_trig_result", "0");
       if (fpga.v.veryverbose) std::cout << "Trig result inverting: " << trig_parse(v) << std::endl;
       ct.invert_result(v);
     }
     if (input.exists("-invert_int")) {
       auto v = parse_uint32(input, "-invert_int", "0");
       if (fpga.v.veryverbose) std::cout << "Trig int inverting: " << trig_parse(v) << std::endl;
       ct.invert_int(v);
     }
     if (input.exists("-invert_ext")) {
       auto v = parse_uint32(input, "-invert_ext", "0");
       if (fpga.v.veryverbose) std::cout << "Trig ext inverting: " << trig_parse(v) << std::endl;
       ct.invert_ext(v);
     }
     if (input.exists("-invert_misc")) {
       auto v = parse_uint32(input, "-invert_misc", "0");
       if (fpga.v.veryverbose) std::cout << "Trig misc inverting: " << trig_parse(v) << std::endl;
       ct.invert_misc(v);
     }
     if (input.exists("-mask_int")) {
       auto v = parse_uint32(input, "-mask_int", "0");
       if (fpga.v.veryverbose) std::cout << "Trig int mask: " << trig_parse(v) << std::endl;
       ct.mask_int(v);
     }
     if (input.exists("-mask_ext")) {
       auto v = parse_uint32(input, "-mask_ext", "0");
       if (fpga.v.veryverbose) std::cout << "Trig ext mask: " << trig_parse(v) << std::endl;
       ct.mask_ext(v);
     }
     if (input.exists("-mask_misc")) {
       auto v = parse_uint32(input, "-mask_misc", "0");
       if (fpga.v.veryverbose) std::cout << "Trig misc mask: " << trig_parse(v) << std::endl;
       ct.mask_misc(v);
     }
   }
};

// External trigger status reporting
class trigger_ext : public pio_in {
 public:
   trigger_ext(mm &dev, uintptr_t base) : pio_in(dev, base) {}

   void status() {
     const auto x = read();
     const trigger_t trigger_in = x & TRIGGER_MASK;
     const bool trigger_enable  = x & (1 << PIO1_ENABLE);
     const bool trigger_force   = x & (1 << PIO1_FORCE);
     const bool trigger_reset   = x & (1 << PIO1_RESET);
     std::cout << "trigger: " << std::bitset<WIDTH_TRIGGER>(trigger_in)
       << (trigger_enable ? " [enable]" : "")
       << (trigger_force  ? " [force]"  : "")
       << (trigger_reset  ? " [reset]"  : "") << std::endl;
   }
};
