
// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#include <iostream>
#include <deque>
#include <sstream>
#include <nanobind/nanobind.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include "ppcommon.hh"
#include "ppmisc.hh"
#include "basic_multi_dma.hh"
#include "combiner.hh"
#include "qout.hh"
#include "trigger.hh"
#include "freq_meter.hh"

namespace nb = nanobind;
using namespace nb::literals;

bool verbose = true;
bool veryverbose = false;
bool debug = false;
bool verbosecheck = false;

int add(int a, int b) { return a + b; }

using IntVector = std::vector<int>;

void check_firmware()
{
  check_version(version);
}

NB_MODULE(pp, m) {
  m.doc() = "PulsePins Python bindings";

  m.attr("the_answer") = 42;

  m.def("check_firmware", &check_firmware);
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

  nb::enum_<el_type>(m, "el_type")
    .value("regular", el_type::regular)
    .value("trigger", el_type::trigger)
    .value("replay",  el_type::replay)
    .value("final",   el_type::final)
    .value("retrig",  el_type::retrig)
    .value("prng",    el_type::prng);

  nb::class_<Counter>(m, "Counter")
    .def(nb::init<count_t>())
    .def("count", &Counter::count)
    .def("count_str", &Counter::count_str)
    .def("control_bits", &Counter::control_bits)
    .def("desc", &Counter::desc);

  nb::class_<Strobe, Counter>(m, "Strobe")
    .def(nb::init<count_t>());

  nb::class_<NoStrobe, Counter>(m, "NoStrobe")
    .def(nb::init<count_t>());

  nb::class_<Value>(m, "Value")
    .def(nb::init<value_t>())
    .def("value", &Value::value)
    .def("value_str", &Value::value_str)
    .def("result", &Value::result)
    .def("mode_bits", &Value::mode_bits)
    .def("desc", &Value::desc);

  nb::class_<BitLoad, Value>(m, "BitLoad")
    .def(nb::init<value_t>());

  nb::class_<BitSet, Value>(m, "BitSet")
    .def(nb::init<value_t>());

  nb::class_<BitClear, Value>(m, "BitClear")
    .def(nb::init<value_t>());

  nb::class_<BitFlip, Value>(m, "BitFlip")
    .def(nb::init<value_t>());

  nb::class_<BitNot, Value>(m, "BitNot")
    .def(nb::init<value_t>());

  nb::class_<BitAnd, Value>(m, "BitAnd")
    .def(nb::init<value_t>());

  nb::class_<BitOr, Value>(m, "BitOr")
    .def(nb::init<value_t>());

  nb::class_<BitXor, Value>(m, "BitXor")
    .def(nb::init<value_t>());

  nb::class_<BitXnor, Value>(m, "BitXnor")
    .def(nb::init<value_t>());

  nb::class_<BitSll, Value>(m, "BitSll")
    .def(nb::init<value_t>());

  nb::class_<BitSrl, Value>(m, "BitSrl")
    .def(nb::init<value_t>());

  nb::class_<TriggerCondition, Value>(m, "TriggerCondition")
    .def(nb::init<trigger_t, trigger_t, bool>())
    .def("mode_bits", &TriggerCondition::mode_bits)
    .def("desc", &TriggerCondition::desc)
    .def("value_str", &TriggerCondition::value_str);

  nb::class_<Replay>(m, "Replay")
    .def(nb::init<>());

  nb::class_<Retrig>(m, "Retrig")
    .def(nb::init<>());

  nb::class_<PseudoRandom>(m, "PseudoRandom")
    .def(nb::init<>());

  nb::class_<el>(m, "el")
    .def(nb::init<value_t>(),
         nb::arg("_v") = 0)
    .def(nb::init<count_t, value_t>())
    .def(nb::init<const Counter &, value_t>())
    .def(nb::init<const Counter &, const Value &>())
    .def(nb::init<trigger_t, trigger_t, bool>())
    .def(nb::init<Replay, count_t, value_t>())
    .def(nb::init<Retrig, value_t>(),
         nb::arg("_r"),
         nb::arg("_v") = default_final_value)
    .def(nb::init<PseudoRandom, count_t>())
    .def_static("classify_control", &el::classify_control)
    .def_static("from_raw_triplet", &el::from_raw_triplet,
         nb::arg("control"),
         nb::arg("count"),
         nb::arg("value"))
    .def_static("from_regular_token", [](const std::string &token, count_t count, value_t value, const std::string &context) {
         return context.empty() ? el::from_regular_token(token, count, value)
                                : el::from_regular_token(token, count, value, context.c_str());
       },
         nb::arg("token"),
         nb::arg("count"),
         nb::arg("value"),
         nb::arg("context") = "")
    .def_static("is_regular_token", &el::is_regular_token)
    .def("control", &el::control)
    .def("count", &el::count)
    .def("value", &el::value)
    .def("kind", &el::kind)
    .def("mode", &el::mode)
    .def("no_strobe", &el::no_strobe)
    .def("regular_token", &el::regular_token)
    .def("is_stored", &el::is_stored)
    .def("store_slot", &el::store_slot)
    .def("trigger_pattern", &el::trigger_pattern)
    .def("trigger_mask", &el::trigger_mask)
    .def("trigger_is_final", &el::trigger_is_final)
    .def("stored_in", &el::stored_in)
    .def("with_control", &el::with_control)
    .def("with_count", &el::with_count)
    .def("with_counter", &el::with_counter)
    .def("with_regular_value", &el::with_regular_value)
    .def("as_bitload_after", &el::as_bitload_after)
    .def("sequence_record", &el::sequence_record)
    .def("store", &el::store)
    .def("set_control", &el::set_control)
    .def("set_count", nb::overload_cast<count_t>(&el::set_count))
    .def("set_count", nb::overload_cast<const Counter &>(&el::set_count))
    .def("set_value", &el::set_value)
    .def("updated_value", &el::updated_value)
    .def("is_regular", &el::is_regular)
    .def("is_trigger", &el::is_trigger)
    .def("is_replay", &el::is_replay)
    .def("is_final", &el::is_final)
    .def("is_retrig", &el::is_retrig)
    .def("is_prng", &el::is_prng)
    .def("decode", &el::decode)
    .def("desc", &el::desc)
    .def("__eq__", [](const el &a, const el &b) {
            return a == b;
        })
    .def("__ne__", [](const el &a, const el &b) {
            return a != b;
        })
    .def("__repr__",
      [](const el &p) { return p.desc(); });

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

  nb::class_<streamer_control>(m, "streamer_control")
    .def(nb::init<mm &, std::uintptr_t>(), nb::keep_alive<1, 2>())
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

  nb::class_<streamer_dma, c_dma>(m, "streamer_dma")
    .def(nb::init<mm &, std::uintptr_t, std::uintptr_t, std::uintptr_t, size_t, const Verbosity &>(),
         nb::keep_alive<1, 2>(),
         nb::keep_alive<1, 7>())
    .def("write_element", &streamer_dma::write_element)
    .def("prepare", &streamer_dma::prepare)
    .def("verify", &streamer_dma::verify)
    .def("report", &streamer_dma::report)
    .def("transfer", &streamer_dma::transfer)
    .def("send_sequence", &streamer_dma::send_sequence);

  nb::class_<streamer_fifo, fifo>(m, "streamer_fifo")
    .def(nb::init<mm &, std::uintptr_t, std::uintptr_t>(), nb::keep_alive<1, 2>())
    .def("out", &streamer_fifo::out)
    .def("check_fill_status", &streamer_fifo::check_fill_status)
    .def("report", &streamer_fifo::report)
    .def("send_sequence", &streamer_fifo::send_sequence);

  nb::class_<readback>(m, "readback")
    .def(nb::init<FPGA &, mm &, std::uintptr_t, std::uintptr_t, std::uintptr_t>(),
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

  // pio
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

  // sequence
  nb::class_<Sequence>(m, "Sequence")
    .def(nb::init<>())
    .def("push_back", &Sequence::push_back_py)
    .def("length", &Sequence::length)
    .def("data_size", &Sequence::data_size)
    .def("dump", &Sequence::dump_py)
    .def("convert_to_BitLoad", &Sequence::convert_to_BitLoad)
    .def("merge", &Sequence::merge)
    .def("load_VCD", &Sequence::load_VCD,
         "filename"_a,
         "target_name"_a = "outs",
         "scale_factor"_a = 10)
    .def("write_VCD_file", &Sequence::write_VCD_file,
         "filename"_a,
         "target_name"_a = "outs",
         "timescale"_a = "1ns")
    .def("write_binary_file", &Sequence::write_binary_file,
         "filename"_a,
         "force_trigger"_a = false);

  m.def("parse_sequence_text", [](const std::string &text) {
    std::istringstream in(text);
    return parse_sequence_from_stream(in);
  });

  m.def("write_sequence_text", [](const Sequence &seq, const bool include_force_trigger) {
    std::ostringstream out;
    write_sequence_to_stream(seq, out, include_force_trigger);
    return out.str();
  },
  "seq"_a,
  "include_force_trigger"_a = false);

  m.def("read_sequence_binary", [](const std::string &filename) {
    return Sequence::read_binary_file(filename);
  });

  nb::class_<pll>(m, "pll")
    .def(nb::init<mm &, std::uintptr_t>(), nb::keep_alive<1, 2>())
    .def("report", &pll::report)
    .def("set_N", &pll::set_N)
    .def("set_M", &pll::set_M)
    .def("set_C", &pll::set_C);

  // pll_clk
  nb::class_<pll_core_clk>(m, "pll_core_clk")
    .def(nb::init<mm &>(), nb::keep_alive<1, 2>())
    .def("set_core_clk", &pll_core_clk::set_core_clk);

  nb::class_<pll_int_clk>(m, "pll_int_clk")
    .def(nb::init<mm &>(), nb::keep_alive<1, 2>())
    .def("set_int_clk", &pll_int_clk::set_int_clk);

  // basic_multi_dma
  nb::class_<basic_streamer>(m, "basic_streamer")
    .def(nb::init<const InputParser &, FPGA &, const std::uintptr_t, const std::uintptr_t, const std::uintptr_t>(),
         nb::keep_alive<1, 3>())
    .def("set_initial_value", &basic_streamer::set_initial_value);

  nb::class_<streamer, basic_streamer>(m, "streamer")
    .def(nb::init<const InputParser &, FPGA &, const std::uintptr_t>(), nb::keep_alive<1, 3>());

  nb::class_<dma_streamer, streamer>(m, "dma_streamer")
    .def(nb::init<const InputParser &, FPGA &>(), nb::keep_alive<1, 3>());

  nb::class_<multistreamer>(m, "multistreamer")
    .def(nb::init<const InputParser &, FPGA &>(), nb::keep_alive<1, 3>());

  // combiner
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

  // config.h
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

  // counter
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
    .def(nb::init<const InputParser&, FPGA&, const std::uintptr_t>(),
         "input"_a, "fpga"_a, "base"_a = COUNTER_Q_BASE,
         nb::keep_alive<1, 3>())
    .def("read", &counter::read, "instr"_a, "part"_a, "addr"_a)
    .def("reset_all", &counter::reset_all)
    .def("latch_all", &counter::latch_all)
    .def("report", &counter::report)
    .def("short_report", &counter::short_report);

  // ppcommon
  m.attr("force_trigger") = force_trigger;
  m.attr("do_not_force_trigger") = do_not_force_trigger;

  // qout
  nb::enum_<comb_mode>(m, "comb_mode")
    .value("SEL1", comb_mode::SEL1)
    .value("SEL2", comb_mode::SEL2)
    .value("SEL3", comb_mode::SEL3)
    .value("SEL4", comb_mode::SEL4)
    .value("AND",  comb_mode::AND)
    .value("OR",   comb_mode::OR)
    .value("XOR",  comb_mode::XOR)
    .value("XNOR", comb_mode::XNOR)
    .value("MAJ",  comb_mode::MAJ)
    .value("BLOCK8",  comb_mode::BLOCK8)
    .value("BLOCK16", comb_mode::BLOCK16)
    .value("SUM12",   comb_mode::SUM12)
    .value("SUM1234", comb_mode::SUM1234)
    .value("DIFF12",  comb_mode::DIFF12)
    .export_values();

  nb::class_<combiner_qout, combiner>(m, "combiner_qout")
    .def(nb::init<const mm&, const std::uintptr_t>(), nb::keep_alive<1, 2>());

  nb::class_<qout>(m, "qout")
    .def(nb::init<const InputParser&, const Verbosity&, FPGA&>(),
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

  // st_mux
  nb::class_<st_mux>(m, "StMux")
    .def(nb::init<const mm&, const Verbosity&, const std::uintptr_t>(),
         nb::keep_alive<1, 2>(),
         nb::keep_alive<1, 3>())
    .def("channel", &st_mux::channel, "ch"_a)
    .def("ctr1", &st_mux::ctr1)
    .def("ctr2", &st_mux::ctr2)
    .def("report", &st_mux::report);

  // trigger
  nb::class_<trigger>(m, "trigger")
    .def(nb::init<const InputParser&, const FPGA&>(), nb::keep_alive<1, 3>())
    .def("set", &trigger::set, "input"_a);

  nb::class_<trigger_ext, pio_in>(m, "trigger_ext")
    .def(nb::init<mm&, uintptr_t>(), nb::keep_alive<1, 2>())
    .def("status", &trigger_ext::status);
}
