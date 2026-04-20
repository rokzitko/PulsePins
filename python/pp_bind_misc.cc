// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#include <string>
#include <vector>

#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include "pp_bind.hh"

#include "config.h"
#include "ppcommon.hh"
#include "ppmisc.hh"

using namespace nb::literals;

void bind_misc(nb::module_ &m) {
  m.def("check_firmware", []() { check_version(version); });
  m.def("check_version", &check_version);

  nb::class_<Verbosity>(m, "Verbosity")
    .def(nb::init<>())
    .def_rw("verbose", &Verbosity::verbose)
    .def_rw("veryverbose", &Verbosity::veryverbose)
    .def_rw("verbosecheck", &Verbosity::verbosecheck)
    .def("__repr__", [](const Verbosity &v) {
         return "<Verbosity verbose=" + std::string(v.verbose ? "True" : "False") +
           ", veryverbose=" + std::string(v.veryverbose ? "True" : "False") +
           ", verbosecheck=" + std::string(v.verbosecheck ? "True" : "False") + ">";
    });

  nb::class_<InputParser>(m, "InputParser")
    .def(nb::init<std::vector<std::string>>())
    .def("get", &InputParser::get)
    .def("exists", &InputParser::exists)
    .def("get_string", &InputParser::get_string)
    .def("get_double", &InputParser::get_double)
    .def("get_uint32", &InputParser::get_uint32)
    .def("get_uint64", &InputParser::get_uint64)
    .def("add", &InputParser::add)
    .def("add_with_arg", &InputParser::add_with_arg);

  m.def("set_verbosity", &set_verbosity, "Parse input flags and return a Verbosity struct");

  m.attr("P_FIFO_IN1") = P_FIFO_IN1;
  m.attr("P_FIFO_IN2") = P_FIFO_IN2;
  m.attr("P_FIFO_OUT") = P_FIFO_OUT;
  m.attr("SIZE_FIFO_IN1") = SIZE_FIFO_IN1;
  m.attr("SIZE_FIFO_IN2") = SIZE_FIFO_IN2;
  m.attr("SIZE_FIFO_OUT") = SIZE_FIFO_OUT;
  m.attr("almost_shift") = almost_shift;
  m.attr("max_size") = max_size;

  m.attr("WIDTH_AVS_BUS") = WIDTH_AVS_BUS;
  m.attr("WIDTH_PORT") = WIDTH_PORT;

  m.attr("WIDTH_CONTROL") = WIDTH_CONTROL;
  m.attr("WIDTH_COUNTER") = WIDTH_COUNTER;
  m.attr("WIDTH_DATA") = WIDTH_DATA;
  m.attr("WIDTH_TOTAL") = WIDTH_TOTAL;
  m.attr("BYTES_TOTAL") = BYTES_TOTAL;

  m.attr("max_count_t") = max_count_t;

  m.attr("WIDTH_TRIGGER") = WIDTH_TRIGGER;
  m.attr("TRIGGER_MASK") = TRIGGER_MASK;

  m.attr("WIDTH_AUX") = WIDTH_AUX;
  m.attr("AUX_MASK") = AUX_MASK;

  m.attr("PIO1_ENABLE") = PIO1_ENABLE;
  m.attr("PIO1_FORCE") = PIO1_FORCE;
  m.attr("PIO1_RESET") = PIO1_RESET;

  m.attr("STROBE") = STROBE;
  m.attr("TRIGGERBITS") = TRIGGERBITS;
  m.attr("TRIGGER") = TRIGGER;
  m.attr("TRIGGERFINAL") = TRIGGERFINAL;
  m.attr("TERMINATE") = TERMINATE;
  m.attr("NOSTROBE") = NOSTROBE;

  m.attr("MODEBITS") = MODEBITS;

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

  m.attr("PASS") = PASS;
  m.attr("NOPASS") = NOPASS;
  m.attr("DISCARD") = DISCARD;
  m.attr("STORE") = STORE;
  m.attr("REPLAY") = REPLAY;
  m.attr("RETRIG") = RETRIG;
  m.attr("PRNG") = PRNG;

  m.attr("POSITIONS") = POSITIONS;
  m.attr("SHIFT_POSITION") = SHIFT_POSITION;
  m.attr("POSITIONS_MASK") = POSITIONS_MASK;

  m.attr("IF_CTRL") = IF_CTRL;
  m.attr("INIT_VAL") = INIT_VAL;
  m.attr("QOUT_OVERRIDE") = QOUT_OVERRIDE;
  m.attr("GATING_W") = GATING_W;

  m.attr("IF_STATUS") = IF_STATUS;
  m.attr("EXT_TRIG_IN") = EXT_TRIG_IN;
  m.attr("QOUT_STREAMER") = QOUT_STREAMER;
  m.attr("EXT_TRIG_CTRL") = EXT_TRIG_CTRL;
  m.attr("QOUT") = QOUT;
  m.attr("FIFO_OVERFLOW") = FIFO_OVERFLOW;
  m.attr("GATING_R") = GATING_R;

  m.attr("INPUT_FIFO1_CTR_IN_L") = INPUT_FIFO1_CTR_IN_L;
  m.attr("INPUT_FIFO1_CTR_IN_H") = INPUT_FIFO1_CTR_IN_H;
  m.attr("INPUT_FIFO1_CTR_OUT_L") = INPUT_FIFO1_CTR_OUT_L;
  m.attr("INPUT_FIFO1_CTR_OUT_H") = INPUT_FIFO1_CTR_OUT_H;
  m.attr("INPUT_FIFO2_CTR_IN_L") = INPUT_FIFO2_CTR_IN_L;
  m.attr("INPUT_FIFO2_CTR_IN_H") = INPUT_FIFO2_CTR_IN_H;
  m.attr("INPUT_FIFO2_CTR_OUT_L") = INPUT_FIFO2_CTR_OUT_L;
  m.attr("INPUT_FIFO2_CTR_OUT_H") = INPUT_FIFO2_CTR_OUT_H;
  m.attr("OUTPUT_FIFO_CTR_IN_L") = OUTPUT_FIFO_CTR_IN_L;
  m.attr("OUTPUT_FIFO_CTR_IN_H") = OUTPUT_FIFO_CTR_IN_H;
  m.attr("OUTPUT_FIFO_CTR_OUT_L") = OUTPUT_FIFO_CTR_OUT_L;
  m.attr("OUTPUT_FIFO_CTR_OUT_H") = OUTPUT_FIFO_CTR_OUT_H;

  m.attr("BUFFER_ERROR") = BUFFER_ERROR;
  m.attr("DONE") = DONE;
  m.attr("TRIGGERED") = TRIGGERED;
  m.attr("ARMED") = ARMED;

  m.attr("STOP") = STOP;
  m.attr("TRIGGER_FORCE_INT") = TRIGGER_FORCE_INT;
  m.attr("TRIGGER_ENABLE_INT") = TRIGGER_ENABLE_INT;
  m.attr("RESET") = RESET;
  m.attr("TRIGGER_RESET_INT") = TRIGGER_RESET_INT;
  m.attr("QOUT_SELECT") = QOUT_SELECT;
  m.attr("STOP_ON_BUFFER_ERROR") = STOP_ON_BUFFER_ERROR;

  m.attr("EXT_TRIG_CTRL_ENABLE") = EXT_TRIG_CTRL_ENABLE;
  m.attr("EXT_TRIG_CTRL_FORCE") = EXT_TRIG_CTRL_FORCE;
  m.attr("EXT_TRIG_CTRL_RESET") = EXT_TRIG_CTRL_RESET;

  m.attr("TRIG_CTRL_ENABLE") = TRIG_CTRL_ENABLE;
  m.attr("TRIG_CTRL_FORCE") = TRIG_CTRL_FORCE;
  m.attr("TRIG_CTRL_RESET") = TRIG_CTRL_RESET;

  m.attr("default_final_value") = default_final_value;
  m.attr("force_trigger") = true;
  m.attr("do_not_force_trigger") = false;
}
