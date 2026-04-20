// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#include <cstdint>

#include <nanobind/stl/string.h>

#include "pp_bind.hh"

#include "fpga.hh"
#include "freq_meter.hh"
#include "pll_clk.hh"
#include "tidbit.hh"

using namespace nb::literals;

void bind_hw_base(nb::module_ &m) {
  nb::class_<loc>(m, "loc")
    .def(nb::init<std::uintptr_t>())
    .def("write", &loc::write)
    .def("read", &loc::read)
    .def("get_ptr", &loc::get_ptr);

  nb::class_<mm>(m, "mm")
    .def(nb::init<std::uintptr_t, std::uintptr_t>())
    .def("get_ptr", &mm::get_ptr)
    .def("get_loc", &mm::get_loc);

  nb::class_<MGR>(m, "MGR")
    .def(nb::init<mm &, const Verbosity &>(),
         nb::keep_alive<1, 2>(),
         nb::keep_alive<1, 3>())
    .def("status", &MGR::status)
    .def("gpio_write", &MGR::gpio_write)
    .def("gpio_read", &MGR::gpio_read);

  nb::class_<hpsled>(m, "hpsled")
    .def(nb::init<mm &, std::uintptr_t>(),
         "dev"_a,
         "base"_a = std::uintptr_t(0xFF709000),
         nb::keep_alive<1, 2>())
    .def("on", &hpsled::on)
    .def("off", &hpsled::off);

  nb::class_<FPGA>(m, "FPGA")
    .def(nb::init<const Verbosity &>(), nb::keep_alive<1, 2>())
    .def("status", &FPGA::status)
    .def("set_streamer_clk", &FPGA::set_streamer_clk)
    .def("output_enable", &FPGA::output_enable);

  nb::class_<freq_meter>(m, "freq_meter")
    .def(nb::init<mm &, const std::uintptr_t, bool>(), nb::keep_alive<1, 2>());

  nb::class_<pp_freq_meter>(m, "pp_freq_meter")
    .def(nb::init<InputParser &, FPGA &, const bool>(), nb::keep_alive<1, 3>())
    .def("report", &pp_freq_meter::report);

  nb::class_<sysid>(m, "sysid")
    .def(nb::init<mm &, const std::uintptr_t>(), nb::keep_alive<1, 2>())
    .def(nb::init<mm &, const std::uintptr_t, const uint32_t>(), nb::keep_alive<1, 2>())
    .def("get_id", &sysid::get_id);

  nb::class_<fifo>(m, "fifo")
    .def(nb::init<mm &, std::uintptr_t, std::uintptr_t>(), nb::keep_alive<1, 2>())
    .def("read", &fifo::read)
    .def("write", &fifo::write)
    .def("status", &fifo::status)
    .def("event", &fifo::event)
    .def("check", &fifo::check)
    .def("fill", &fifo::fill)
    .def("clear_fifo", &fifo::clear_fifo);

  nb::class_<pio>(m, "pio")
    .def(nb::init<mm &, std::uintptr_t>(), nb::keep_alive<1, 2>());

  nb::class_<pio_out, pio>(m, "pio_out")
    .def(nb::init<mm &, std::uintptr_t>(), nb::keep_alive<1, 2>())
    .def("write", &pio_out::write)
    .def("read", &pio_out::read);

  nb::class_<pio_in, pio>(m, "pio_in")
    .def(nb::init<mm &, std::uintptr_t>(), nb::keep_alive<1, 2>())
    .def("read", &pio_in::read)
    .def("mask", &pio_in::mask)
    .def("edgecapture", &pio_in::edgecapture)
    .def("edgecapture_clear", &pio_in::edgecapture_clear);

  nb::class_<pll>(m, "pll")
    .def(nb::init<mm &, std::uintptr_t>(), nb::keep_alive<1, 2>())
    .def("report", &pll::report)
    .def("set_N", &pll::set_N)
    .def("set_M", &pll::set_M)
    .def("set_C", &pll::set_C);

  nb::class_<pll_core_clk>(m, "pll_core_clk")
    .def(nb::init<mm &>(), nb::keep_alive<1, 2>())
    .def("set_core_clk", &pll_core_clk::set_core_clk);

  nb::class_<pll_int_clk>(m, "pll_int_clk")
    .def(nb::init<mm &>(), nb::keep_alive<1, 2>())
    .def("set_int_clk", &pll_int_clk::set_int_clk);
}
