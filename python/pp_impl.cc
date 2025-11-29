// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#include <iostream>
#include <deque>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include "ppcommon.hh"

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(pp_impl, m) {
  m.attr("STROBE") = STROBE;
  m.attr("NOSTROBE") = NOSTROBE;

  m.attr("strobestring") = strobestring;
  m.attr("nostrobestring") = nostrobestring;

  m.attr("BITLOAD") = BITLOAD;
  m.attr("BITSET") = BITSET;
  m.attr("BITCLEAR") = BITCLEAR;
  m.attr("BITFLIP") = BITFLIP;
  m.attr("BITNOT") = BITNOT;
  m.attr("BITAND") = BITAND;
  m.attr("BITOR") = BITOR;
  m.attr("BITXOR") = BITXOR;
  m.attr("BITXNOR") = BITXNOR;
  m.attr("BITSLL") = BITSLL;
  m.attr("BITSRL") = BITSRL;

  m.attr("bitloadstring") = bitloadstring;
  m.attr("bitsetstring") = bitsetstring;
  m.attr("bitclearstring") = bitclearstring;
  m.attr("bitflipstring") = bitflipstring;
  m.attr("bitnotstring") = bitnotstring;
  m.attr("bitandstring") = bitandstring;
  m.attr("bitorstring") = bitorstring;
  m.attr("bitxorstring") = bitxorstring;
  m.attr("bitxnorstring") = bitxnorstring;
  m.attr("bitsllstring") = bitsllstring;
  m.attr("bitsrlstring") = bitsrlstring;

  m.attr("WIDTH_TRIGGER") = WIDTH_TRIGGER;
  m.attr("TRIGGER") = TRIGGER;
  m.attr("TRIGGERFINAL") = TRIGGERFINAL;
  m.attr("TRIGGER_MASK") = TRIGGER_MASK;

  m.attr("triggerstring") = triggerstring;
  m.attr("finalstring") = finalstring;

  m.attr("TERMINATE") = TERMINATE;
  m.attr("REPLAY") = REPLAY;

  m.attr("POSITIONS") = POSITIONS;
  m.attr("STORE") = STORE;
  m.attr("SHIFT_POSITION") = SHIFT_POSITION;

  m.attr("LWHPSFPGA_OFST") = LWHPSFPGA_OFST;
  m.attr("LWHPSFPGA_END") = LWHPSFPGA_END;
  m.attr("LWH2F_RANGE") = LWH2F_RANGE;

  m.attr("HPSFPGA_OFST") = HPSFPGA_OFST;
  m.attr("HPSFPGA_END") = HPSFPGA_END;
  m.attr("H2F_RANGE") = H2F_RANGE;

  m.attr("SYSID_BASE") = SYSID_BASE;
  m.attr("SYSID_ID") = SYSID_ID;

  m.attr("ST_INTERFACE_1_BASE") = ST_INTERFACE_1_BASE;
  m.attr("FIFO_1_IN_BASE") = FIFO_1_IN_BASE;
  m.attr("FIFO_1_IN_CSR_BASE") = FIFO_1_IN_CSR_BASE;

  m.attr("FIFO_RL_OUT_BASE") = FIFO_RL_OUT_BASE;
  m.attr("FIFO_RL_IN_CSR_BASE") = FIFO_RL_IN_CSR_BASE;
  m.attr("RL_ENCODER_IF_BASE") = RL_ENCODER_IF_BASE;

  m.attr("PIO_TRIG_INT_BASE") = PIO_TRIG_INT_BASE;

  m.attr("PLL_RECONFIG_CORE_CLK_BASE") = PLL_RECONFIG_CORE_CLK_BASE;
  m.attr("PLL_RECONFIG_INT_CLK_BASE") = PLL_RECONFIG_INT_CLK_BASE;
}
