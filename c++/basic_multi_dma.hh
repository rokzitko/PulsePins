// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Higher-level streamer interfaces and multi-streamer combinations

#pragma once

#include <iostream>
#include <string>

#include "tidbit.hh"

#include "fpga.hh"
#include "streamer.hh"
#include "parser.hh"
#include "pll_clk.hh"
#include "pll_rules.hh"
#include "st_mux.hh"

// Streamer interface + asssociated FIFO
class basic_streamer {
 public:
   FPGA &fpga;
   streamer_fifo fifo;
   streamer_control sc;

   basic_streamer(const InputParser &input,
                  FPGA &_fpga,
                  const std::uintptr_t fifo_base = FIFO_1_IN_BASE,
                  const std::uintptr_t fifo_csr_base = FIFO_1_IN_CSR_BASE,
                  const std::uintptr_t st_if_base = ST_INTERFACE_1_BASE) :
     fpga(_fpga),
     fifo(fpga.dev_h2f, fifo_base, fifo_csr_base),
     sc(fpga.dev_h2f, st_if_base) {
       if (input.exists("-stop_on_buffer_error") || input.exists("-sobe")) {
         sc.stop_on_buffer_error(true);
         if (fpga.v.veryverbose)
           std::cout << blue << "stop_on_buffer_error enabled." << rst << std::endl;
       }
       if (fpga.v.veryverbose)
         std::cout << "basic_streamer[0x" << std::hex << fifo_base << ",0x" << fifo_csr_base
         << ",0x"<< st_if_base << "]" << std::endl;
     }

   void set_initial_value(const InputParser &input, const std::string param_name = "-i") {
     const auto initial_value = parse_value(input, param_name, "0");
     if (initial_value != 0) // report non-default initial value
       std::cout << "initial_value(" << param_name << ")=" << initial_value << std::endl;
     sc.set_initial_value(initial_value);
   }
};

// High-level interface, single core, with multiplexer (using FIFO transport mechanism)
class streamer : public basic_streamer, public pll_core_clk, public pll_int_clk {
 public:
   FPGA &fpga;
   rstmgr rm;
   st_mux mux; // Avalon ST multiplexer; default is channel 1 (FIFO)

   streamer(const InputParser &input,
            FPGA &_fpga,
            const std::uintptr_t st_mux_base = ST_MUX_1_BASE) :
     basic_streamer(input, _fpga),
     pll_core_clk(_fpga),
     pll_int_clk(_fpga),
     fpga (_fpga),
     mux(fpga.dev_h2f, fpga.v, st_mux_base) {
       rm.s2f_reset();              // FPGA fabric reset
       set_initial_value(input);    // set initial value before streamer reset is performed
       set_core_clk(input, fpga.v); // set core PLL config
       set_int_clk(input, fpga.v);  // set streamer int_clk PLL config
       fpga.output_enable(true);    // ensure output is enabled
       sc.reset();                  // streamer core reset
     }

   ~streamer() {
     if (fpga.v.veryverbose)
       mux.report();
   }

};

// 512MB offset, 512MB size
constexpr uintptr_t dma_base = 0x20000000UL;
constexpr uintptr_t dma_size = 0x20000000UL;

// High-level interface, single core, with multiplexer (using DMA transport mechanism)
class dma_streamer : public streamer {
 public:
   streamer_dma dma;

   dma_streamer(const InputParser &input, FPGA &_fpga) :
     streamer(input, _fpga),
     dma(fpga.dev_h2f, MSGDMA_1_CSR_BASE, MSGDMA_1_DESCRIPTOR_SLAVE_BASE, dma_base, dma_size) {
       mux.channel(2); // DMA
     }
};

// High-level interface, four cores
class multistreamer : public pll_core_clk, public pll_int_clk {
 public:
   FPGA &fpga;
   rstmgr rm;
   basic_streamer s1, s2, s3, s4;

   multistreamer(const InputParser &input, FPGA &_fpga) :
     pll_core_clk(_fpga),
     pll_int_clk(_fpga),
     fpga(_fpga),
     s1(input, fpga, FIFO_1_IN_BASE, FIFO_1_IN_CSR_BASE, ST_INTERFACE_1_BASE),
     s2(input, fpga, FIFO_2_IN_BASE, FIFO_2_IN_CSR_BASE, ST_INTERFACE_2_BASE),
     s3(input, fpga, FIFO_3_IN_BASE, FIFO_3_IN_CSR_BASE, ST_INTERFACE_3_BASE),
     s4(input, fpga, FIFO_4_IN_BASE, FIFO_4_IN_CSR_BASE, ST_INTERFACE_4_BASE)
     {
       rm.s2f_reset(); // FPGA fabric reset
       set_core_clk(input, fpga.v);
       set_int_clk(input, fpga.v);
       fpga.output_enable(true); // ensure output is enabled
       s1.set_initial_value(input, "-i1");
       s1.sc.reset();
       s2.set_initial_value(input, "-i2");
       s2.sc.reset();
       s3.set_initial_value(input, "-i3");
       s3.sc.reset();
       s4.set_initial_value(input, "-i4");
       s4.sc.reset();
     }
};
