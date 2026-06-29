// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#include <cstdint>

#include <nanobind/stl/string.h>

#include "pp_bind.hh"

#include "basic_multi_dma.hh"
#include "readback.hh"
#include "streamer_control.hh"
#include "streamer_dma.hh"
#include "streamer_fifo.hh"

using namespace nb::literals;

void bind_streaming(nb::module_ &m) {
  nb::class_<streamer_control>(m, "streamer_control")
    .def("__init__", [](streamer_control *self,
                         const mm &dev,
                         const std::uintptr_t base) {
           new (self) streamer_control(dev, pp_bind_h2f_region(base));
         },
         "dev"_a,
         "base"_a,
         nb::keep_alive<1, 2>())
    .def("status", &streamer_control::status)
    .def("get_control", &streamer_control::get_control)
    .def("get_overflow", &streamer_control::get_overflow)
    .def("get_qout", &streamer_control::get_qout)
    .def("get_qout_streamer", &streamer_control::get_qout_streamer)
    .def("get_input_fifo1_ctr_in", &streamer_control::get_input_fifo1_ctr_in)
    .def("get_input_fifo1_ctr_out", &streamer_control::get_input_fifo1_ctr_out)
    .def("get_input_fifo2_ctr_in", &streamer_control::get_input_fifo2_ctr_in)
    .def("get_input_fifo2_ctr_out", &streamer_control::get_input_fifo2_ctr_out)
    .def("get_output_fifo_ctr_in", &streamer_control::get_output_fifo_ctr_in)
    .def("get_output_fifo_ctr_out", &streamer_control::get_output_fifo_ctr_out)
    .def("statistics", &streamer_control::statistics)
    .def("monitor_ext_trig", &streamer_control::monitor_ext_trig)
    .def("set_initial_value", &streamer_control::set_initial_value)
    .def("set_qout_override", &streamer_control::set_qout_override)
    .def("buffer_error", &streamer_control::buffer_error)
    .def("done", &streamer_control::done)
    .def("wait_to_complete", &streamer_control::wait_to_complete)
    .def("status_report", &streamer_control::status_report)
    .def("reset_streamer", &streamer_control::reset_streamer, "usleep_time"_a = 10,
        "Reset the interface")
    .def("reset", &streamer_control::reset, "usleep_time"_a = 10,
        "Full reset")
    .def("stop", &streamer_control::stop)
    .def("trigger_enable", &streamer_control::trigger_enable)
    .def("trigger_force", &streamer_control::trigger_force)
    .def("trigger_reset", &streamer_control::trigger_reset)
    .def("qout_select", &streamer_control::qout_select)
    .def("stop_on_buffer_error", &streamer_control::stop_on_buffer_error)
    .def("qout_set", &streamer_control::qout_set)
    .def("gating", &streamer_control::gating)
    .def("gate_status_string_from_x", &streamer_control::gate_status_string_from_x)
    .def("gate_status", &streamer_control::gate_status)
    .def("gate_status_string", &streamer_control::gate_status_string)
    .def("set_gating_from_string", &streamer_control::set_gating_from_string);

  nb::class_<c_dma>(m, "c_dma")
    .def(nb::init<mm &, std::uintptr_t, std::uintptr_t, bool>(), nb::keep_alive<1, 2>())
    .def("status_string", &c_dma::status_string)
    .def("status", &c_dma::status)
    .def("write_fill", &c_dma::write_fill)
    .def("clear_status", &c_dma::clear_status)
    .def("control_setbit", &c_dma::control_setbit)
    .def("control_clearbit", &c_dma::control_clearbit)
    .def("reset", &c_dma::reset)
    .def("wait", &c_dma::wait)
    .def("initiate_transfer", &c_dma::initiate_transfer)
    .def("enqueue", &c_dma::enqueue)
    .def("enqueue_src_addr", &c_dma::enqueue_src_addr)
    .def("enqueue_dest_addr", nb::overload_cast<const uintmax_t, const uint32_t, const uint32_t>(&c_dma::enqueue_dest_addr))
    .def("enqueue_dest_addr", nb::overload_cast<const ram_block &>(&c_dma::enqueue_dest_addr))
    .def("send", nb::overload_cast<const uintmax_t, const uintmax_t, const uint32_t>(&c_dma::send))
    .def("send", nb::overload_cast<const uintmax_t, const uint32_t>(&c_dma::send))
    .def("read_in_chunks", &c_dma::read_in_chunks);

  nb::class_<streamer_dma>(m, "streamer_dma")
    .def("__init__", [](streamer_dma *self,
                         const mm &dev,
                         const std::uintptr_t csr_base,
                         const std::uintptr_t descriptor_base,
                         const std::uintptr_t ram_addr,
                         const size_t max_size,
                         const Verbosity &v) {
           new (self) streamer_dma(dev,
                                   pp_bind_h2f_region(csr_base),
                                   pp_bind_h2f_region(descriptor_base),
                                   ram_addr,
                                   max_size,
                                   v);
         },
         "dev"_a,
         "csr_base"_a,
         "descriptor_base"_a,
         "ram_addr"_a,
         "max_size"_a,
         "verbosity"_a,
         nb::keep_alive<1, 2>(),
         nb::keep_alive<1, 7>())
    .def("write_element", &streamer_dma::write_element)
    .def("prepare", &streamer_dma::prepare)
    .def("verify", &streamer_dma::verify)
    .def("report", &streamer_dma::report)
    .def("transfer", &streamer_dma::transfer)
    .def("send_sequence", &streamer_dma::send_sequence,
         "elements"_a,
         "verify_buffer"_a = true);

  nb::class_<streamer_fifo>(m, "streamer_fifo")
    .def("__init__", [](streamer_fifo *self,
                         const mm &dev,
                         const std::uintptr_t base,
                         const std::uintptr_t in_csr_base) {
           new (self) streamer_fifo(dev,
                                    pp_bind_h2f_region(base),
                                    pp_bind_h2f_region(in_csr_base));
         },
         "dev"_a,
         "base"_a,
         "in_csr_base"_a,
         nb::keep_alive<1, 2>())
    .def("out", &streamer_fifo::out,
         "e"_a,
         "dump"_a = false)
    .def("check_fill_status", &streamer_fifo::check_fill_status)
    .def("report", &streamer_fifo::report)
    .def("send_sequence", &streamer_fifo::send_sequence);

  nb::class_<readback>(m, "readback")
    .def("__init__", [](readback *self,
                         FPGA &fpga,
                         const mm &dev,
                         const std::uintptr_t base,
                         const std::uintptr_t csr_base,
                         const std::uintptr_t control_base) {
           new (self) readback(fpga,
                               dev,
                               pp_bind_h2f_region(base),
                               pp_bind_h2f_region(csr_base),
                               pp_bind_h2f_region(control_base));
         },
         "fpga"_a,
         "dev"_a,
         "base"_a,
         "csr_base"_a,
         "control_base"_a,
         nb::keep_alive<1, 2>(),
         nb::keep_alive<1, 3>())
    .def("check_fill_status", &readback::check_fill_status)
    .def("filled", &readback::filled)
    .def("clear_fifo", &readback::clear_fifo)
    .def("reset", &readback::reset)
    .def("mode", &readback::mode)
    .def("status_report", &readback::status_report)
    .def("overflow", &readback::overflow)
    .def("read", &readback::read)
    .def("read_all", static_cast<void (readback::*)(const double)>(&readback::read_all))
    .def("check", static_cast<bool (readback::*)(Sequence, const double)>(&readback::check));

  nb::class_<basic_streamer>(m, "basic_streamer")
    .def("__init__", [](basic_streamer *self,
                         const InputParser &input,
                         FPGA &fpga,
                         const std::uintptr_t fifo_base,
                         const std::uintptr_t fifo_csr_base,
                         const std::uintptr_t st_if_base) {
           new (self) basic_streamer(input,
                                     fpga,
                                     pp_bind_h2f_region(fifo_base),
                                     pp_bind_h2f_region(fifo_csr_base),
                                     pp_bind_h2f_region(st_if_base));
         },
         "input"_a,
         "fpga"_a,
         "fifo_base"_a,
         "fifo_csr_base"_a,
         "st_if_base"_a,
         nb::keep_alive<1, 3>())
    .def("set_initial_value", &basic_streamer::set_initial_value);

  nb::class_<streamer, basic_streamer>(m, "streamer")
    .def("__init__", [](streamer *self,
                         const InputParser &input,
                         FPGA &fpga,
                         const std::uintptr_t st_mux_base) {
           new (self) streamer(input, fpga, pp_bind_h2f_region(st_mux_base));
         },
         "input"_a,
         "fpga"_a,
         "st_mux_base"_a = ST_MUX_1_BASE,
         nb::keep_alive<1, 3>());

  nb::class_<dma_streamer, streamer>(m, "dma_streamer")
    .def(nb::init<const InputParser &, FPGA &>(), nb::keep_alive<1, 3>());

  nb::class_<multistreamer>(m, "multistreamer")
    .def(nb::init<const InputParser &, FPGA &>(), nb::keep_alive<1, 3>());
}
