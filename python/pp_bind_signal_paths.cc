// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#include <cstdint>

#include <nanobind/stl/string.h>

#include "pp_bind.hh"

#include "combiner.hh"
#include "qout.hh"
#include "st_mux.hh"
#include "trigger.hh"

using namespace nb::literals;

void bind_signal_paths(nb::module_ &m) {
  nb::class_<combiner>(m, "combiner")
    .def(nb::init<mm &, std::uintptr_t>(), nb::keep_alive<1, 2>())
    .def("mode", &combiner::mode)
    .def("get_mode", &combiner::get_mode)
    .def("cfg", &combiner::cfg)
    .def("get_cfg", &combiner::get_cfg)
    .def("invert", &combiner::invert)
    .def("get_invert", &combiner::get_invert)
    .def("mask", &combiner::mask)
    .def("get_mask", &combiner::get_mask)
    .def("value", &combiner::value)
    .def("get_value", &combiner::get_value)
    .def("release_force", &combiner::release_force)
    .def("force", &combiner::force)
    .def("get_force", &combiner::get_force)
    .def("out", &combiner::out)
    .def("in1", &combiner::in1)
    .def("in2", &combiner::in2)
    .def("in3", &combiner::in3)
    .def("in4", &combiner::in4)
    .def("rb_force", &combiner::rb_force)
    .def("rb_port", &combiner::rb_port)
    .def("report", &combiner::report)
    .def("self_test", &combiner::self_test);

  nb::enum_<trig_mode>(m, "trig_mode")
    .value("INT", trig_mode::INT)
    .value("EXT", trig_mode::EXT)
    .value("MISC", trig_mode::MISC)
    .value("AUX", trig_mode::AUX)
    .value("AND", trig_mode::AND)
    .value("OR", trig_mode::OR)
    .value("XOR", trig_mode::XOR);

  nb::class_<combiner_trig, combiner>(m, "combiner_trig")
    .def(nb::init<mm &, std::uintptr_t>(), nb::keep_alive<1, 2>())
    .def("mode", &combiner::mode)
    .def("invert_int", &combiner_trig::invert_int)
    .def("invert_ext", &combiner_trig::invert_ext)
    .def("invert_misc", &combiner_trig::invert_misc)
    .def("invert_result", &combiner_trig::invert_result)
    .def("mask_int", &combiner_trig::mask_int)
    .def("mask_ext", &combiner_trig::mask_ext)
    .def("mask_misc", &combiner_trig::mask_misc);

  nb::enum_<comb_mode>(m, "comb_mode")
    .value("SEL1", comb_mode::SEL1)
    .value("SEL2", comb_mode::SEL2)
    .value("SEL3", comb_mode::SEL3)
    .value("SEL4", comb_mode::SEL4)
    .value("AND", comb_mode::AND)
    .value("OR", comb_mode::OR)
    .value("XOR", comb_mode::XOR)
    .value("XNOR", comb_mode::XNOR)
    .value("MAJ", comb_mode::MAJ)
    .value("BLOCK8", comb_mode::BLOCK8)
    .value("BLOCK16", comb_mode::BLOCK16)
    .value("SUM12", comb_mode::SUM12)
    .value("SUM1234", comb_mode::SUM1234)
    .value("DIFF12", comb_mode::DIFF12)
    .export_values();

  nb::class_<combiner_qout, combiner>(m, "combiner_qout")
    .def(nb::init<const mm &, const std::uintptr_t>(), nb::keep_alive<1, 2>());

  nb::class_<qout>(m, "qout")
    .def(nb::init<const InputParser &, const Verbosity &, FPGA &>(),
         nb::keep_alive<1, 3>(),
         nb::keep_alive<1, 4>())
    .def("set", &qout::set, "input"_a)
    .def("in1", &qout::in1)
    .def("in2", &qout::in2)
    .def("in3", &qout::in3)
    .def("in4", &qout::in4)
    .def("out", &qout::out);

  m.def("combine", &combine,
        "mode"_a, "y1"_a, "y2"_a, "y3"_a, "y4"_a);

  nb::class_<st_mux>(m, "StMux")
    .def(nb::init<const mm &, const Verbosity &, const std::uintptr_t>(),
         nb::keep_alive<1, 2>(),
         nb::keep_alive<1, 3>())
    .def("channel", &st_mux::channel, "ch"_a)
    .def("ctr1", &st_mux::ctr1)
    .def("ctr2", &st_mux::ctr2)
    .def("report", &st_mux::report);

  nb::class_<trigger>(m, "trigger")
    .def(nb::init<const InputParser &, const FPGA &>(), nb::keep_alive<1, 3>())
    .def("set", &trigger::set, "input"_a);

  nb::class_<trigger_ext, pio_in>(m, "trigger_ext")
    .def(nb::init<mm &, uintptr_t>(), nb::keep_alive<1, 2>())
    .def("status", &trigger_ext::status);
}
