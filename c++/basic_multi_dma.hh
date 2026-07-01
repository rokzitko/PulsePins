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

#include <cstdint>
#include <iostream>
#include <string>

#include "tidbit.hh"

#include "address_map.hh"
#include "fpga.hh"
#include "options.hh"
#include "streamer.hh"
#include "parser.hh"
#include "st_mux.hh"
#include "combiner.hh"

// Minimal host-side view of one streamer instance plus its FIFO transport.
class basic_streamer {
public:
  FPGA &fpga;
  streamer_fifo fifo;
  streamer_control sc;

  basic_streamer(const StreamerOptions &opts,
                  FPGA &_fpga,
                  const address_map::H2fRegion fifo_base,
                  const address_map::H2fRegion fifo_csr_base,
                  const address_map::H2fRegion st_if_base,
                  std::string name = "streamer") :
    fpga(_fpga),
    fifo(fpga.dev_h2f, fifo_base, fifo_csr_base, name),
    sc(fpga.dev_h2f, st_if_base, name) {
      if (opts.stop_on_buffer_error) {
        sc.stop_on_buffer_error(true);
        if (fpga.v.veryverbose)
          std::cout << blue << "stop_on_buffer_error enabled." << rst << std::endl;
      }
      if (fpga.v.veryverbose)
        std::cout << "basic_streamer[0x" << std::hex << fifo_base.base << ",0x" << fifo_csr_base.base
        << ",0x"<< st_if_base.base << "]" << std::endl;
    }

  basic_streamer(const InputParser &input,
                  FPGA &_fpga,
                  const address_map::H2fRegion fifo_base = address_map::h2f::fifo_1_in,
                  const address_map::H2fRegion fifo_csr_base = address_map::h2f::fifo_1_in_csr,
                  const address_map::H2fRegion st_if_base = address_map::h2f::st_interface_1,
                  std::string name = "streamer"s) :
    basic_streamer(resolve_streamer_options(input), _fpga, fifo_base, fifo_csr_base, st_if_base, name) {}

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

  void reset_for_active_clock(const int periods = 2) {
    sc.reset_with_wait([this, periods] { fpga.sleep_for_at_least_n_streamer_periods(periods); });
  }
};

// Single-stream bring-up helper using the FIFO transport path.
class streamer : public basic_streamer {
public:
  FPGA &fpga;
  st_mux mux; // Avalon ST multiplexer; default is channel 1 (FIFO)

  streamer(const StreamerOptions &opts,
            FPGA &_fpga,
            const address_map::H2fRegion st_mux_base = address_map::h2f::st_mux_1) :
    basic_streamer(opts, _fpga, address_map::h2f::fifo_1_in, address_map::h2f::fifo_1_in_csr, address_map::h2f::st_interface_1),
    fpga (_fpga),
    mux(fpga.dev_h2f, fpga.v, st_mux_base) {
      // Bring-up order matters:
      //   1. program the idle output value
      //   2. ensure physical outputs are enabled
      //   3. select the FIFO transport path; the mux state can persist across commands
      //   4. restore the single-stream output combiner route
      //   5. reset the streamer core so it starts from that known idle state
      fpga.streamer_done_config(0b0000, 0b0000);
      basic_streamer::set_initial_value_opts(opts);
      fpga.output_enable(true);
      mux.channel(1);
      combiner(fpga.dev_h2f, address_map::h2f::combiner_qout, "combiner_qout").reset_passthrough();
      reset_for_active_clock();
      fpga.streamer_done_config(0b0001, 0b0001);
      fpga.sleep_for_at_least_n_streamer_periods(4);
    }

  streamer(const InputParser &input,
            FPGA &_fpga,
            const address_map::H2fRegion st_mux_base = address_map::h2f::st_mux_1) :
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
    dma(fpga.dev_h2f, address_map::h2f::msgdma_1_csr, address_map::h2f::msgdma_1_descriptor_slave, dma_base, dma_size, fpga.v) {
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
  uint8_t active_mask;
  uint8_t armed_live_mask;
  basic_streamer s1, s2, s3, s4;

  multistreamer(const StreamerOptions &s1_opts,
                const StreamerOptions &s2_opts,
                const StreamerOptions &s3_opts,
                const StreamerOptions &s4_opts,
                FPGA &_fpga,
                const StreamerDoneOptions done_opts = {}) :
    fpga(_fpga),
    active_mask(done_opts.active_mask),
    armed_live_mask(done_opts.armed_live_mask),
    s1(s1_opts, fpga, address_map::h2f::fifo_1_in, address_map::h2f::fifo_1_in_csr, address_map::h2f::st_interface_1, "streamer1"),
    s2(s2_opts, fpga, address_map::h2f::fifo_2_in, address_map::h2f::fifo_2_in_csr, address_map::h2f::st_interface_2, "streamer2"),
    s3(s3_opts, fpga, address_map::h2f::fifo_3_in, address_map::h2f::fifo_3_in_csr, address_map::h2f::st_interface_3, "streamer3"),
    s4(s4_opts, fpga, address_map::h2f::fifo_4_in, address_map::h2f::fifo_4_in_csr, address_map::h2f::st_interface_4, "streamer4")
    {
      // Each core is configured independently, but outputs are enabled once globally.
      // Initial values are set before each per-core reset for the same reason as in the
      // single-stream helper.
      fpga.streamer_done_config(0b0000, 0b0000);
      fpga.output_enable(true);
      s1.set_initial_value_opts(s1_opts, "-i1");
      s1.reset_for_active_clock();
      s2.set_initial_value_opts(s2_opts, "-i2");
      s2.reset_for_active_clock();
      s3.set_initial_value_opts(s3_opts, "-i3");
      s3.reset_for_active_clock();
      s4.set_initial_value_opts(s4_opts, "-i4");
      s4.reset_for_active_clock();
      fpga.streamer_done_config(active_mask, armed_live_mask);
      fpga.sleep_for_at_least_n_streamer_periods(4);
    }

  multistreamer(const InputParser &input, FPGA &_fpga) :
    multistreamer(resolve_streamer_options(input, "-i1"),
                  resolve_streamer_options(input, "-i2"),
                  resolve_streamer_options(input, "-i3"),
                  resolve_streamer_options(input, "-i4"),
                  _fpga,
                  resolve_streamer_done_options(input)) {}
};
