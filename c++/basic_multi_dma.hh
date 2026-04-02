// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Higher-level streamer interfaces and transport/topology combinations.
//
// This header is the main host-side transport-selection seam for streamer-oriented tools.
// It combines three concerns that are intentionally kept separate in lower layers:
//   - `streamer_control` for register-level control/status of a streamer instance
//   - a transport (`streamer_fifo` or `streamer_dma`) for delivering sequence elements
//   - optional topology glue such as the Avalon-ST mux or four-stream composition
//
// The constructors here have important bring-up side effects: they may set initial output
// values, enable the board outputs, select the active transport, and reset streamer cores.
// Architectural overview lives in `c++/README.md` and `docs/docs/cpp.md`.

#pragma once

#include <iostream>
#include <string>

#include "tidbit.hh"

#include "fpga.hh"
#include "options.hh"
#include "streamer.hh"
#include "parser.hh"
#include "st_mux.hh"

// Minimal host-side view of one streamer instance plus its FIFO transport.
class basic_streamer {
public:
  FPGA &fpga;
  streamer_fifo fifo;
  streamer_control sc;

  basic_streamer(const StreamerOptions &opts,
                  FPGA &_fpga,
                  const std::uintptr_t fifo_base,
                  const std::uintptr_t fifo_csr_base,
                  const std::uintptr_t st_if_base) :
    fpga(_fpga),
    fifo(fpga.dev_h2f, fifo_base, fifo_csr_base),
    sc(fpga.dev_h2f, st_if_base) {
      if (opts.stop_on_buffer_error) {
        sc.stop_on_buffer_error(true);
        if (fpga.v.veryverbose)
          std::cout << blue << "stop_on_buffer_error enabled." << rst << std::endl;
      }
      if (fpga.v.veryverbose)
        std::cout << "basic_streamer[0x" << std::hex << fifo_base << ",0x" << fifo_csr_base
        << ",0x"<< st_if_base << "]" << std::endl;
    }

  basic_streamer(const InputParser &input,
                  FPGA &_fpga,
                  const std::uintptr_t fifo_base = FIFO_1_IN_BASE,
                  const std::uintptr_t fifo_csr_base = FIFO_1_IN_CSR_BASE,
                  const std::uintptr_t st_if_base = ST_INTERFACE_1_BASE) :
    basic_streamer(resolve_streamer_options(input), _fpga, fifo_base, fifo_csr_base, st_if_base) {}

  // Initial value must be programmed before the streamer is reset if the caller expects the
  // post-reset idle output state to match that configured value.
  void set_initial_value_opts(const StreamerOptions &opts, const std::string &param_name = "-i") {
    if (opts.report_initial_value)
      std::cout << "initial_value(" << param_name << ")=" << opts.initial_value << std::endl;
    sc.set_initial_value(opts.initial_value);
  }

  void set_initial_value(const InputParser &input, const std::string param_name = "-i") {
    set_initial_value_opts(resolve_streamer_options(input, param_name), param_name);
  }
};

// Single-stream bring-up helper using the FIFO transport path.
class streamer : public basic_streamer {
public:
  FPGA &fpga;
  st_mux mux; // Avalon ST multiplexer; default is channel 1 (FIFO)

  streamer(const StreamerOptions &opts,
            FPGA &_fpga,
            const std::uintptr_t st_mux_base = ST_MUX_1_BASE) :
    basic_streamer(opts, _fpga, FIFO_1_IN_BASE, FIFO_1_IN_CSR_BASE, ST_INTERFACE_1_BASE),
    fpga (_fpga),
    mux(fpga.dev_h2f, fpga.v, st_mux_base) {
      // Bring-up order matters:
      //   1. program the idle output value
      //   2. ensure physical outputs are enabled
      //   3. reset the streamer core so it starts from that known idle state
      basic_streamer::set_initial_value_opts(opts);
      fpga.output_enable(true);
      sc.reset();
    }

  streamer(const InputParser &input,
            FPGA &_fpga,
            const std::uintptr_t st_mux_base = ST_MUX_1_BASE) :
    streamer(resolve_streamer_options(input), _fpga, st_mux_base) {}

  ~streamer() {
    if (fpga.v.veryverbose)
      mux.report();
  }

};

// 512MB offset, 512MB size
constexpr uintptr_t dma_base = 0x20000000UL;
constexpr uintptr_t dma_size = 0x20000000UL;

// Single-stream helper that swaps the transport from FIFO to DMA via the ST mux.
class dma_streamer : public streamer {
public:
  streamer_dma dma;

  dma_streamer(const StreamerOptions &opts, FPGA &_fpga) :
    streamer(opts, _fpga),
    dma(fpga.dev_h2f, MSGDMA_1_CSR_BASE, MSGDMA_1_DESCRIPTOR_SLAVE_BASE, dma_base, dma_size, fpga.v) {
      // The underlying streamer bring-up comes from `streamer`; this constructor only needs
      // to redirect the ST mux so the DMA engine becomes the active producer.
      mux.channel(2);
    }

  dma_streamer(const InputParser &input, FPGA &_fpga) :
    dma_streamer(resolve_streamer_options(input), _fpga) {}
};

// Four-stream composition helper used by tools that coordinate multiple streamer cores.
class multistreamer {
public:
  FPGA &fpga;
  basic_streamer s1, s2, s3, s4;

  multistreamer(const StreamerOptions &s1_opts,
                const StreamerOptions &s2_opts,
                const StreamerOptions &s3_opts,
                const StreamerOptions &s4_opts,
                FPGA &_fpga) :
    fpga(_fpga),
    s1(s1_opts, fpga, FIFO_1_IN_BASE, FIFO_1_IN_CSR_BASE, ST_INTERFACE_1_BASE),
    s2(s2_opts, fpga, FIFO_2_IN_BASE, FIFO_2_IN_CSR_BASE, ST_INTERFACE_2_BASE),
    s3(s3_opts, fpga, FIFO_3_IN_BASE, FIFO_3_IN_CSR_BASE, ST_INTERFACE_3_BASE),
    s4(s4_opts, fpga, FIFO_4_IN_BASE, FIFO_4_IN_CSR_BASE, ST_INTERFACE_4_BASE)
    {
      // Each core is configured independently, but outputs are enabled once globally.
      // Initial values are set before each per-core reset for the same reason as in the
      // single-stream helper.
      fpga.output_enable(true);
      s1.set_initial_value_opts(s1_opts, "-i1");
      s1.sc.reset();
      s2.set_initial_value_opts(s2_opts, "-i2");
      s2.sc.reset();
      s3.set_initial_value_opts(s3_opts, "-i3");
      s3.sc.reset();
      s4.set_initial_value_opts(s4_opts, "-i4");
      s4.sc.reset();
    }

  multistreamer(const InputParser &input, FPGA &_fpga) :
    multistreamer(resolve_streamer_options(input, "-i1"),
                  resolve_streamer_options(input, "-i2"),
                  resolve_streamer_options(input, "-i3"),
                  resolve_streamer_options(input, "-i4"),
                  _fpga) {}
};
