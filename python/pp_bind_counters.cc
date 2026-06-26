// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#include <cstddef>

#include <nanobind/stl/function.h>
#include <nanobind/stl/string.h>

#include "pp_bind.hh"

#include "counter.hh"

using namespace nb::literals;

void bind_counter_bindings(nb::module_ &m) {
  nb::class_<basic_counter>(m, "basic_counter")
    .def(nb::init<readfnc>(), "rd"_a)
    .def("read", &basic_counter::read, "addr"_a)
    .def("total", &basic_counter::total)
    .def("l", &basic_counter::l)
    .def("h", &basic_counter::h)
    .def("lh", &basic_counter::lh)
    .def("hl", &basic_counter::hl)
    .def("str", &basic_counter::str)
    .def("__repr__", &basic_counter::str);

  nb::class_<runs_counter>(m, "runs_counter")
    .def(nb::init<readfnc>(), "rd"_a)
    .def("read", &runs_counter::read, "addr"_a)
    .def("ctr_run", &runs_counter::ctr_run)
    .def("nr_run_l", &runs_counter::nr_run_l)
    .def("nr_run_h", &runs_counter::nr_run_h)
    .def("sum_run_l", &runs_counter::sum_run_l)
    .def("sum_run_h", &runs_counter::sum_run_h)
#ifdef DO_SUM2
    .def("sum2_run_l", &runs_counter::sum2_run_l)
    .def("sum2_run_h", &runs_counter::sum2_run_h)
#endif
    .def("max_run_l", &runs_counter::max_run_l)
    .def("max_run_h", &runs_counter::max_run_h)
    .def("nr_glitch_l", &runs_counter::nr_glitch_l)
    .def("nr_glitch_h", &runs_counter::nr_glitch_h)
    .def("str", &runs_counter::str)
    .def("__repr__", &runs_counter::str);

  nb::class_<packet_stats>(m, "packet_stats")
    .def(nb::init<readfnc>(), "rd"_a)
    .def("read", &packet_stats::read, "addr"_a)
    .def("total", &packet_stats::total)
    .def("valid", &packet_stats::valid)
    .def("idle", &packet_stats::idle)
    .def("pkt_begin", &packet_stats::pkt_begin)
    .def("pkt_end", &packet_stats::pkt_end)
    .def("pkt_len_sum", &packet_stats::pkt_len_sum)
    .def("pkt_len_sum2", &packet_stats::pkt_len_sum2)
    .def("str", &packet_stats::str)
    .def("__repr__", &packet_stats::str);

  nb::class_<seq_counter>(m, "seq_counter")
    .def(nb::init<size_t, readfnc>(), "nr"_a, "rd"_a)
    .def("read", &seq_counter::read, "addr"_a)
    .def("ctr", &seq_counter::ctr, "seq"_a)
    .def("str", &seq_counter::str)
    .def("__repr__", &seq_counter::str);

  nb::class_<autocorrelation>(m, "autocorrelation")
    .def(nb::init<size_t, readfnc>(), "nr"_a, "rd"_a)
    .def("read", &autocorrelation::read, "addr"_a)
    .def("ctr", &autocorrelation::ctr, "seq"_a)
    .def("str", &autocorrelation::str)
    .def("__repr__", &autocorrelation::str);

  nb::class_<crosscorrelation>(m, "crosscorrelation")
    .def(nb::init<size_t, readfnc>(), "nr"_a, "rd"_a)
    .def("read", &crosscorrelation::read, "addr"_a)
    .def("ctr", &crosscorrelation::ctr, "seq"_a)
    .def("str", &crosscorrelation::str)
    .def("__repr__", &crosscorrelation::str);

  nb::class_<counter>(m, "counter")
    .def("__init__", [](counter *self,
                         const InputParser &input,
                         FPGA &fpga,
                         const std::uintptr_t base) {
           new (self) counter(input, fpga, pp_bind_h2f_region(base));
         },
         "input"_a, "fpga"_a, "base"_a = COUNTER_Q_BASE,
         nb::keep_alive<1, 3>())
    .def("read", &counter::read, "instr"_a, "part"_a, "addr"_a)
    .def("reset_all", &counter::reset_all)
    .def("latch_all", &counter::latch_all)
    .def("report", &counter::report)
    .def("short_report", &counter::short_report);
}
