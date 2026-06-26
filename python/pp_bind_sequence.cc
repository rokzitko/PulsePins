// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#include <sstream>
#include <string>

#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>

#include "pp_bind.hh"

#include "elements.hh"
#include "sequence.hh"

using namespace nb::literals;

void bind_sequence(nb::module_ &m) {
  nb::enum_<el_type>(m, "el_type")
    .value("regular", el_type::regular)
    .value("trigger", el_type::trigger)
    .value("replay", el_type::replay)
    .value("final", el_type::final)
    .value("retrig", el_type::retrig)
    .value("prng", el_type::prng);

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
    .def("__repr__", [](const el &p) { return p.desc(); });

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
         "timescale"_a = default_vcd_timescale)
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
}
