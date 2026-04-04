// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Unit test infrastructure. This one runs on host (not on target), i.e., it is *not* cross-compiled.
// The purpose is to test very low-level (basic infrastructure) functions and classes.
// https://github.com/doctest/doctest
// https://github.com/doctest/doctest/blob/master/doc/markdown/tutorial.md

#include <fstream>
#include <iostream>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "include/doctest.h"

#include "elements.hh"
#include "PMODDA3.hh"
#include "SPI.hh"
#include "options.hh"
#include "ppworkflow.hh"
#include "sequence.hh"
#include "streamer.hh"
#include "vcd_parser.hh"

count_t get_count(const Counter &x) { return x.count(); }
control_t get_control_bits(const Counter &x) { return x.control_bits(); }

bool contains(const std::string &str, const std::string &substr) {
  return str.find(substr) != std::string::npos;
}

InputParser make_input(std::initializer_list<std::string> args) {
  return InputParser(std::vector<std::string>(args));
}

TEST_CASE("counter class") {
  count_t c = 10;
  Counter c1(c);
  CHECK(c1.count() == c);
  CHECK(get_count(c1) == c);
  CHECK(c1.control_bits() == 0);
  CHECK(get_control_bits(c1) == 0);
  CHECK(c1.desc() == "");
  CHECK(contains(c1.count_str(), "[           10]"));
}

TEST_CASE("strobe class") {
  count_t c = 10;
  Strobe c1(c);
  CHECK(c1.count() == c);
  CHECK(get_count(c1) == c);
  CHECK(c1.control_bits() == STROBE);
  CHECK(get_control_bits(c1) == STROBE);
  CHECK(c1.desc() == strobestring);
  CHECK(contains(c1.count_str(), "[           10]"));
}

TEST_CASE("no_strobe class") {
  count_t c = 10;
  NoStrobe c1(c);
  CHECK(c1.count() == c);
  CHECK(get_count(c1) == c);
  CHECK(c1.control_bits() == NOSTROBE);
  CHECK(get_control_bits(c1) == NOSTROBE);
  CHECK(c1.desc() == nostrobestring);
  CHECK(contains(c1.count_str(), "[           10]"));
}

value_t get_value(const Value &x) { return x.value(); }
value_t get_result(const Value &x, const value_t v_prev) { return x.result(v_prev); }
control_t get_mode_bits(const Value &x) { return x.mode_bits(); }
std::string get_desc(const Value &x) { return x.desc(); }

TEST_CASE("BitLoad class") {
  value_t v = 42;
  BitLoad v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITLOAD);
  CHECK(get_mode_bits(v1) == BITLOAD);
  CHECK(v1.desc() == bitloadstring);
  CHECK(get_desc(v1) == bitloadstring);
  CHECK(v1.result(0x00) == v);
  CHECK(v1.result(0xff) == v);
  CHECK(get_result(v1, 0x00) == v);
  CHECK(get_result(v1, 0xff) == v);
  CHECK(contains(v1.value_str(), "[           42]"));
}

TEST_CASE("BitSet class") {
  value_t v = 42;
  BitSet v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITSET);
  CHECK(get_mode_bits(v1) == BITSET);
  CHECK(v1.desc() == bitsetstring);
  CHECK(get_desc(v1) == bitsetstring);
  CHECK(v1.result(0) == v);
  CHECK(v1.result(1) == (v | 1));
  CHECK(get_result(v1, 0x00) == v);
  CHECK(get_result(v1, 0x01) == (v | 1));
  CHECK(contains(v1.value_str(), "[           42]"));
}

TEST_CASE("BitClear class") {
  value_t v = 42;
  BitClear v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITCLEAR);
  CHECK(get_mode_bits(v1) == BITCLEAR);
  CHECK(v1.desc() == bitclearstring);
  CHECK(get_desc(v1) == bitclearstring);
  CHECK(v1.result(0) == 0);
  CHECK(v1.result(1) == 1);
  CHECK(v1.result(v) == 0);
  CHECK(get_result(v1, 0x00) == 0);
  CHECK(get_result(v1, 0x01) == 1);
  CHECK(get_result(v1, v) == 0);
  CHECK(contains(v1.value_str(), "[           42]"));
}

TEST_CASE("BitFlip class") {
  value_t v = 42;
  BitFlip v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITFLIP);
  CHECK(get_mode_bits(v1) == BITFLIP);
  CHECK(v1.desc() == bitflipstring);
  CHECK(get_desc(v1) == bitflipstring);
  CHECK(v1.result(0) == v);
  CHECK(v1.result(1) == (v|1));
  CHECK(v1.result(v) == 0);
  CHECK(get_result(v1, 0x00) == v);
  CHECK(get_result(v1, 0x01) == (v|1));
  CHECK(get_result(v1, v) == 0);
  CHECK(contains(v1.value_str(), "[           42]"));
}

TEST_CASE("el general constructor 1") {
  count_t c = 10;
  value_t v = 42;
  Counter c1(c);
  Value v1(v);
  el e(el_type::regular, c1, v1);
  CHECK(e.count() == c);
  CHECK(e.value() == v);
  CHECK(e.control() == 0);
  CHECK(contains(e.desc(), "[           10]"));
  CHECK(contains(e.desc(), "[           42]"));
}

TEST_CASE("el general constructor 2") {
  count_t c = 10;
  value_t v = 42;
  control_t y = TERMINATE;
  Counter c1(c);
  Value v1(v);
  el e(el_type::regular, c1, v1, y);
  CHECK(e.count() == c);
  CHECK(e.value() == v);
  CHECK(e.control() == y);
  CHECK(contains(e.desc(), "[           10]"));
  CHECK(contains(e.desc(), "[           42]"));
}

TEST_CASE("el terminate constructor") {
  el e;
  CHECK(e.count() == 1);
  CHECK(e.value() == default_final_value);
  CHECK(e.control() == TERMINATE);
  CHECK(e.is_final());
  CHECK(contains(e.desc(), finalstring));
}

TEST_CASE("el terminate constructor with non-default final value") {
  value_t v = 42;
  el e(v);
  CHECK(e.count() == 1);
  CHECK(e.value() == v);
  CHECK(e.control() == TERMINATE);
  CHECK(e.is_final());
  CHECK(contains(e.desc(), finalstring));
}

TEST_CASE("el regular element constructor, load") {
  count_t c = 10;
  value_t v = 42;
  el e(c, v);
  CHECK(e.count() == c);
  CHECK(e.value() == v);
  CHECK(e.control() == BITLOAD);
  CHECK(!e.is_final());
  CHECK(e.is_regular());
  CHECK(contains(e.desc(), bitloadstring));
  CHECK(contains(e.desc(), "[           10]"));
  CHECK(contains(e.desc(), "[           42]"));
}

TEST_CASE("el regular element constructor, set") {
  count_t c = 10;
  value_t v = 42;
  el e(c, BitSet(v));
  CHECK(e.count() == c);
  CHECK(e.value() == v);
  CHECK(e.control() == BITSET);
  CHECK(!e.is_final());
  CHECK(e.is_regular());
  CHECK(contains(e.desc(), bitsetstring));
  CHECK(contains(e.desc(), "[           10]"));
  CHECK(contains(e.desc(), "[           42]"));
}

TEST_CASE("el regular element constructor, clear") {
  count_t c = 10;
  value_t v = 42;
  el e(c, BitClear(v));
  CHECK(e.count() == c);
  CHECK(e.value() == v);
  CHECK(e.control() == BITCLEAR);
  CHECK(!e.is_final());
  CHECK(e.is_regular());
  CHECK(contains(e.desc(), bitclearstring));
  CHECK(contains(e.desc(), "[           10]"));
  CHECK(contains(e.desc(), "[           42]"));
}

TEST_CASE("el regular element constructor, flip") {
  count_t c = 10;
  value_t v = 42;
  el e(c, BitFlip(v));
  CHECK(e.count() == c);
  CHECK(e.value() == v);
  CHECK(e.control() == BITFLIP);
  CHECK(!e.is_final());
  CHECK(e.is_regular());
  CHECK(contains(e.desc(), bitflipstring));
  CHECK(contains(e.desc(), "[           10]"));
  CHECK(contains(e.desc(), "[           42]"));
}

TEST_CASE("el regular element constructor, strobe") {
  count_t c = 10;
  value_t v = 42;
  el e(Strobe(c), v);
  CHECK(e.count() == c);
  CHECK(e.value() == v);
  CHECK(e.control() == (BITLOAD | STROBE));
  CHECK(!e.is_final());
  CHECK(e.is_regular());
  CHECK(contains(e.desc(), bitloadstring));
  CHECK(contains(e.desc(), strobestring));
  CHECK(contains(e.desc(), "[           10]"));
  CHECK(contains(e.desc(), "[           42]"));
}

TEST_CASE("el regular element constructor, nostrobe") {
  count_t c = 10;
  value_t v = 42;
  el e(NoStrobe(c), v);
  CHECK(e.count() == c);
  CHECK(e.value() == v);
  CHECK(e.control() == (BITLOAD | NOSTROBE));
  CHECK(!e.is_final());
  CHECK(e.is_regular());
  CHECK(contains(e.desc(), bitloadstring));
  CHECK(contains(e.desc(), nostrobestring));
  CHECK(contains(e.desc(), "[           10]"));
  CHECK(contains(e.desc(), "[           42]"));
}

TEST_CASE("trigger constructor") {
  trigger_t p = 85;
  trigger_t m = 42;
  el e(p, m, false);
  CHECK(e.count() == 0);
  CHECK(e.value() == (((value_t)m << WIDTH_TRIGGER) | (value_t)p));
  CHECK(e.control() == TRIGGER);
  CHECK(!e.is_regular());
  CHECK(contains(e.desc(), "0x55")); // 85 in hex
  CHECK(contains(e.desc(), "0x2a")); // 42 in hex
  CHECK(contains(e.desc(), "1010101")); // 85 in binary

}

TEST_CASE("trigger constructor, final") {
  trigger_t p = 85;
  trigger_t m = 42;
  el e(p, m, true);
  CHECK(e.count() == 0);
  CHECK(e.value() == (((value_t)m << WIDTH_TRIGGER) | (value_t)p));
  CHECK(e.control() == (TRIGGER | TRIGGERFINAL));
  CHECK(!e.is_regular());
  CHECK(contains(e.desc(), finalstring));
  CHECK(contains(e.desc(), "0x55")); // 85 in hex
  CHECK(contains(e.desc(), "0x2a")); // 42 in hex
  CHECK(contains(e.desc(), "1010101")); // 85 in binary
}

TEST_CASE("convert_to_BitLoad") {
  Sequence s1;
  s1.push_back(el(1, BitLoad(1)));
  s1.push_back(el(2, BitFlip(1)));
  s1.push_back(el(3, BitSet(2)));
  s1.push_back(el(4, BitFlip(4)));
  s1.push_back(el(5, BitClear(2+4)));
  Sequence s2;
  s2.push_back(el(1, BitLoad(1)));
  s2.push_back(el(2, BitLoad(0)));
  s2.push_back(el(3, BitLoad(2)));
  s2.push_back(el(4, BitLoad(6)));
  s2.push_back(el(5, BitLoad(0)));
  auto s1bis = s1.convert_to_BitLoad();
  CHECK(s1bis == s2);
}

TEST_CASE("merge 1") {
  Sequence s1;
  s1.push_back(el(10, 42));
  s1.push_back(el(20, 42));
  Sequence s2;
  s2.push_back(el(30, 42));
  auto s1bis = s1.merge();
  CHECK(s1bis == s2);
}

TEST_CASE("merge 2") {
  Sequence s1;
  s1.push_back(el(10, 42));
  s1.push_back(el(20, 42));
  s1.push_back(el(30, 42));
  Sequence s2;
  s2.push_back(el(60, 42));
  auto s1bis = s1.merge();
  CHECK(s1bis == s2);
}

TEST_CASE("parseVerilogInt") {
  CHECK(parseVerilogInt("8'hFF") == 255);
  CHECK(parseVerilogInt("12'o777") == 511);
  CHECK(parseVerilogInt("'b1010") == 10);
}

TEST_CASE("stripUnderscores") {
  CHECK(std::stoi(stripUnderscores("1_000_000")) == 1000000);
  CHECK(std::stoi(stripUnderscores("_42")) == 42);
  CHECK(std::stoi(stripUnderscores("4_2")) == 42);
  CHECK(std::stoi(stripUnderscores("42_")) == 42);
}

TEST_CASE("parseuint32_t") {
  CHECK(parse_uint32_t("42") == 42);
  CHECK(parse_uint32_t("0xff") == 255);
  CHECK(parse_uint32_t("0xFF") == 255);
  CHECK(parse_uint32_t("0b10000000") == 128);
  CHECK(parse_uint32_t("0b11111111") == 255);
  CHECK(parse_uint32_t("077") == 63); // octal!
  CHECK(parse_uint32_t("8'hFF") == 255);
  CHECK(parse_uint32_t("12'o777") == 511);
  CHECK(parse_uint32_t("'b1010") == 10);
}

TEST_CASE("resolve_clock_selection_options prefers raw -clk") {
  auto input = make_input({"-int_clk", "-ext_clk", "-clk", "3"});
  auto opts = resolve_clock_selection_options(input);

  REQUIRE(opts.source.has_value());
  CHECK(*opts.source == StreamerClockSource::raw_select);
  REQUIRE(opts.raw_select.has_value());
  CHECK(*opts.raw_select == 3);
}

TEST_CASE("resolve_clock_selection_options handles single source flags") {
  SUBCASE("internal") {
    auto opts = resolve_clock_selection_options(make_input({"-int_clk"}));
    REQUIRE(opts.source.has_value());
    CHECK(*opts.source == StreamerClockSource::internal);
    CHECK(!opts.raw_select.has_value());
  }

  SUBCASE("external") {
    auto opts = resolve_clock_selection_options(make_input({"-ext_clk"}));
    REQUIRE(opts.source.has_value());
    CHECK(*opts.source == StreamerClockSource::external);
    CHECK(!opts.raw_select.has_value());
  }
}

TEST_CASE("resolve_core_pll_options captures profile and tuning") {
  auto opts = resolve_core_pll_options(make_input({"-core_pll", "fast", "-core_pll_charge_pump", "2", "-core_pll_bandwidth", "5"}));

  CHECK(opts.profile == "fast");
  REQUIRE(opts.charge_pump.has_value());
  CHECK(*opts.charge_pump == 2);
  REQUIRE(opts.bandwidth.has_value());
  CHECK(*opts.bandwidth == 5);
}

TEST_CASE("resolve_int_pll_options captures profile and tuning") {
  auto opts = resolve_int_pll_options(make_input({"-int_pll", "slow", "-int_pll_charge_pump", "3", "-int_pll_bandwidth", "6"}));

  CHECK(opts.profile == "slow");
  REQUIRE(opts.charge_pump.has_value());
  CHECK(*opts.charge_pump == 3);
  REQUIRE(opts.bandwidth.has_value());
  CHECK(*opts.bandwidth == 6);
}

TEST_CASE("resolve_trigger_options captures mode invert and mask fields") {
  auto opts = resolve_trigger_options(make_input({
      "-trig_any",
      "-invert_trig_result", "1",
      "-invert_int", "2",
      "-invert_ext", "3",
      "-invert_misc", "4",
      "-mask_int", "5",
      "-mask_ext", "6",
      "-mask_misc", "7",
  }));

  REQUIRE(opts.mode.has_value());
  CHECK(*opts.mode == TriggerModeOption::any);
  CHECK(opts.invert_result == std::optional<uint32_t>(1));
  CHECK(opts.invert_int == std::optional<uint32_t>(2));
  CHECK(opts.invert_ext == std::optional<uint32_t>(3));
  CHECK(opts.invert_misc == std::optional<uint32_t>(4));
  CHECK(opts.mask_int == std::optional<uint32_t>(5));
  CHECK(opts.mask_ext == std::optional<uint32_t>(6));
  CHECK(opts.mask_misc == std::optional<uint32_t>(7));
}

TEST_CASE("resolve_streamer_options reports explicit initial value") {
  SUBCASE("default") {
    auto opts = resolve_streamer_options(make_input({}));
    CHECK(!opts.stop_on_buffer_error);
    CHECK(opts.initial_value == 0);
    CHECK(!opts.report_initial_value);
  }

  SUBCASE("explicit initial value") {
    auto opts = resolve_streamer_options(make_input({"-sobe", "-i", "0x2a"}));
    CHECK(opts.stop_on_buffer_error);
    CHECK(opts.initial_value == 0x2a);
    CHECK(opts.report_initial_value);
  }
}

TEST_CASE("resolve_freq_meter_options captures correction factor") {
  SUBCASE("absent") {
    auto opts = resolve_freq_meter_options(make_input({}));
    CHECK(!opts.correction_factor.has_value());
  }

  SUBCASE("present") {
    auto opts = resolve_freq_meter_options(make_input({"-freq_rescale", "1.25"}));
    REQUIRE(opts.correction_factor.has_value());
    CHECK(*opts.correction_factor == doctest::Approx(1.25));
  }
}

TEST_CASE("append_final_output appends explicit final terminator") {
  Sequence seq;
  seq.push_back(el(3, 0x12));

  auto final = append_final_output(seq, make_input({"-t", "0x34"}));

  CHECK(final == 0x34);
  REQUIRE(seq.size() == 2);
  CHECK(seq.back() == el(0x34));
}

TEST_CASE("append_final_output appends final element when -t is absent") {
  Sequence seq;
  seq.push_back(el(3, 0x12));

  auto final = append_final_output(seq, make_input({}));

  REQUIRE(seq.size() == 2);
  CHECK(seq.back().is_final());
  CHECK(seq.back().value() == final);
}

TEST_CASE("drop_count0 removes only zero-count elements") {
  Sequence seq;
  seq.push_back(el(0, 0x10));
  seq.push_back(el(3, 0x11));
  seq.push_back(el(0, 0x12));
  seq.push_back(el(4, 0x13));

  drop_count0(seq);

  REQUIRE(seq.size() == 2);
  CHECK(seq[0] == el(3, 0x11));
  CHECK(seq[1] == el(4, 0x13));
}

TEST_CASE("convert_for_readback_check normalizes effective output stream") {
  Sequence seq;
  seq.push_back(el(1, BitLoad(1)));
  seq.push_back(el(2, BitSet(2)));
  seq.push_back(el(3, BitFlip(1)));
  seq.push_back(el(4, BitLoad(2)));

  Sequence expected;
  expected.push_back(el(1, BitLoad(1)));
  expected.push_back(el(2, BitLoad(3)));
  expected.push_back(el(3, BitLoad(2)));
  expected.push_back(el(4, BitLoad(2)));
  expected = expected.merge();

  convert_for_readback_check(seq);

  CHECK(seq == expected);
}

TEST_CASE("readback_timeout defaults to zero without timeout option") {
  CHECK(readback_timeout(make_input({})) == doctest::Approx(0.0));
}

TEST_CASE("readback_timeout parses explicit timeout") {
  CHECK(readback_timeout(make_input({"-timeout", "0.25"})) == doctest::Approx(0.25));
}

TEST_CASE("VCD parser") {
  std::ifstream F("unit_tests_input/test1.vcd");
  CHECK(F);
  auto v = parseVcdUpdates(F, "outs", 1);
  CHECK(v.size() == 6);
  CHECK(v[0].value == 0);
  CHECK(v[0].count == 0);
  CHECK(v[1].value == 4096);
  CHECK(v[1].count == 1);
  CHECK(v[2].value == 0);
  CHECK(v[2].count == 3);
  CHECK(v[3].value == 15);
  CHECK(v[3].count == 13);
  CHECK(v[4].value == 14);
  CHECK(v[4].count == 33);
  CHECK(v[5].value == 0);
  CHECK(v[5].count == 500);
}

TEST_CASE("write_VCD exports simple BitLoad waveform") {
  Sequence seq;
  seq.push_back(el(3, 0x1));
  seq.push_back(el(2, 0x3));

  std::ostringstream out;
  seq.write_VCD(out);
  const std::string vcd = out.str();

  CHECK(contains(vcd, "$scope module pulsepins $end"));
  CHECK(contains(vcd, "$var reg 32 ! outs [31:0] $end"));
  CHECK(contains(vcd, "#0\n"));
  CHECK(contains(vcd, "b" + value_to_vcd_binary(0x1) + " !\n"));
  CHECK(contains(vcd, "#3\n"));
  CHECK(contains(vcd, "b" + value_to_vcd_binary(0x3) + " !\n"));
}

TEST_CASE("write_VCD merges unchanged adjacent output states") {
  Sequence seq;
  seq.push_back(el(2, 0x5));
  seq.push_back(el(3, 0x5));
  seq.push_back(el(1, 0x7));

  std::ostringstream out;
  seq.write_VCD(out);
  const std::string vcd = out.str();

  const std::string five = "b" + value_to_vcd_binary(0x5) + " !\n";
  CHECK(vcd.find(five) != std::string::npos);
  CHECK(vcd.find(five, vcd.find(five) + 1) == std::string::npos);
  CHECK(contains(vcd, "#5\n"));
  CHECK(contains(vcd, "b" + value_to_vcd_binary(0x7) + " !\n"));
}

TEST_CASE("write_VCD normalizes deterministic regular operators") {
  Sequence seq;
  seq.push_back(el(1, BitLoad(1)));
  seq.push_back(el(2, BitSet(2)));
  seq.push_back(el(3, BitFlip(1)));

  std::ostringstream out;
  seq.write_VCD(out);
  const std::string vcd = out.str();

  CHECK(contains(vcd, "#0\n"));
  CHECK(contains(vcd, "b" + value_to_vcd_binary(1) + " !\n"));
  CHECK(contains(vcd, "#1\n"));
  CHECK(contains(vcd, "b" + value_to_vcd_binary(3) + " !\n"));
  CHECK(contains(vcd, "#3\n"));
  CHECK(contains(vcd, "b" + value_to_vcd_binary(2) + " !\n"));
}

TEST_CASE("write_VCD accepts empty sequence") {
  Sequence seq;

  std::ostringstream out;
  seq.write_VCD(out);
  const std::string vcd = out.str();

  CHECK(contains(vcd, "$scope module pulsepins $end"));
  CHECK(contains(vcd, "#0\n"));
  CHECK(contains(vcd, "b" + value_to_vcd_binary(0) + " !\n"));
}

TEST_CASE("write_VCD rejects trigger elements") {
  Sequence seq;
  seq.push_back(el(0x1, 0x1, true));

  std::ostringstream out;
  CHECK_THROWS_AS(seq.write_VCD(out), std::runtime_error);
}

TEST_CASE("write_VCD rejects replay retrig and prng elements") {
  SUBCASE("replay") {
    Sequence seq;
    seq.push_back(el(Replay{}, 3, 4));
    std::ostringstream out;
    CHECK_THROWS_AS(seq.write_VCD(out), std::runtime_error);
  }
  SUBCASE("retrigger") {
    Sequence seq;
    seq.push_back(el(Retrig{}));
    std::ostringstream out;
    CHECK_THROWS_AS(seq.write_VCD(out), std::runtime_error);
  }
  SUBCASE("prng") {
    Sequence seq;
    seq.push_back(el(PseudoRandom{}, 5));
    std::ostringstream out;
    CHECK_THROWS_AS(seq.write_VCD(out), std::runtime_error);
  }
}

TEST_CASE("BitLoad sequence VCD round-trips through load_VCD") {
  Sequence seq;
  seq.push_back(el(3, 0x1));
  seq.push_back(el(2, 0x3));
  seq.push_back(el(4, 0x0));

  const std::string filename = "unit_tests_output_roundtrip.vcd";
  seq.write_VCD_file(filename, "outs", "1ns");

  Sequence roundtrip;
  roundtrip.load_VCD(filename, "outs", 1);

  CHECK(compare(roundtrip, seq));
  std::remove(filename.c_str());
}

TEST_CASE("parse_sequence_from_stream parses existing grammar") {
  std::istringstream in("d 3 0x12 t 0b01 0b11 f d 4 0x34");
  auto [seq, force_trigger] = parse_sequence_from_stream(in);

  REQUIRE(seq.size() == 3);
  CHECK(force_trigger);

  CHECK(seq[0] == el(3, 0x12));
  CHECK(seq[1] == el(0b01, 0b11, true));
  CHECK(seq[2] == el(4, 0x34));
}

TEST_CASE("parse_sequence_from_stream accepts empty input") {
  std::istringstream in("");
  auto [seq, force_trigger] = parse_sequence_from_stream(in);

  CHECK(seq.empty());
  CHECK(!force_trigger);
}

TEST_CASE("parse_sequence_from_stream handles mixed whitespace") {
  std::istringstream in("  d 3 0x12\n\n tn 0b01 0b01\n   rt\n final 0x34\n");
  auto [seq, force_trigger] = parse_sequence_from_stream(in);

  REQUIRE(seq.size() == 4);
  CHECK(!force_trigger);
  CHECK(seq[0] == el(3, 0x12));
  CHECK(seq[1] == el(0b01, 0b01, false));
  CHECK(seq[2] == el(Retrig{}));
  CHECK(seq[3] == el(0x34));
}

TEST_CASE("parse_sequence_from_stream parses non-final triggers") {
  std::istringstream in("tn 0b01 0b01 t 0b10 0b10");
  auto [seq, force_trigger] = parse_sequence_from_stream(in);

  REQUIRE(seq.size() == 2);
  CHECK(!force_trigger);

  CHECK(seq[0] == el(0b01, 0b01, false));
  CHECK(seq[1] == el(0b10, 0b10, true));
}

TEST_CASE("parse_sequence_from_stream parses stage 2 regular element variants") {
  std::istringstream in("dn 3 0x12 s 4 0x03 c 5 0x0c x 6 0x30");
  auto [seq, force_trigger] = parse_sequence_from_stream(in);

  REQUIRE(seq.size() == 4);
  CHECK(!force_trigger);

  CHECK(seq[0] == el(NoStrobe(3), 0x12));
  CHECK(seq[1] == el(4, BitSet(0x03)));
  CHECK(seq[2] == el(5, BitClear(0x0c)));
  CHECK(seq[3] == el(6, BitFlip(0x30)));
}

TEST_CASE("parse_sequence_from_stream parses stage 3 regular element variants") {
  std::istringstream in("n 7 0xff a 8 0x0f o 9 0xf0 xr 10 0x33 xn 11 0x55 sl 12 3 sr 13 2");
  auto [seq, force_trigger] = parse_sequence_from_stream(in);

  REQUIRE(seq.size() == 7);
  CHECK(!force_trigger);

  CHECK(seq[0] == el(7, BitNot(0xff)));
  CHECK(seq[1] == el(8, BitAnd(0x0f)));
  CHECK(seq[2] == el(9, BitOr(0xf0)));
  CHECK(seq[3] == el(10, BitXor(0x33)));
  CHECK(seq[4] == el(11, BitXnor(0x55)));
  CHECK(seq[5] == el(12, BitSll(3)));
  CHECK(seq[6] == el(13, BitSrl(2)));
}

TEST_CASE("parse_sequence_from_stream parses stage 4 control-flow elements") {
  std::istringstream in("store 2 d 3 0x12 store 1 s 4 0x03 r 5 6 rt pr 7");
  auto [seq, force_trigger] = parse_sequence_from_stream(in);

  REQUIRE(seq.size() == 5);
  CHECK(!force_trigger);

  CHECK(seq[0] == el(3, 0x12).store(2));
  CHECK(seq[1] == el(4, BitSet(0x03)).store(1));
  CHECK(seq[2] == el(Replay{}, 5, 6));
  CHECK(seq[3] == el(Retrig{}));
  CHECK(seq[4] == el(PseudoRandom{}, 7));
}

TEST_CASE("parse_sequence_from_stream parses explicit final terminator") {
  std::istringstream in("d 3 0x12 final 0x34");
  auto [seq, force_trigger] = parse_sequence_from_stream(in);

  REQUIRE(seq.size() == 2);
  CHECK(!force_trigger);

  CHECK(seq[0] == el(3, 0x12));
  CHECK(seq[1] == el(0x34));
}

TEST_CASE("parse_sequence_from_stream rejects unknown tokens") {
  std::istringstream in("bogus");
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in), "Unknown sequence token: 'bogus'", std::runtime_error);
}

TEST_CASE("parse_sequence_from_stream rejects truncated data records") {
  std::istringstream in("d 3");
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in), "Incomplete 'd' record: expected count and value", std::runtime_error);
}

TEST_CASE("parse_sequence_from_stream rejects truncated stage 2 regular element records") {
  std::istringstream in_dn("dn 3");
  std::istringstream in_s("s 4");
  std::istringstream in_c("c 5");
  std::istringstream in_x("x 6");
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_dn), "Incomplete 'dn' record: expected count and value", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_s), "Incomplete 's' record: expected count and value", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_c), "Incomplete 'c' record: expected count and value", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_x), "Incomplete 'x' record: expected count and value", std::runtime_error);
}

TEST_CASE("parse_sequence_from_stream rejects truncated stage 3 regular element records") {
  std::istringstream in_n("n 7");
  std::istringstream in_a("a 8");
  std::istringstream in_o("o 9");
  std::istringstream in_xr("xr 10");
  std::istringstream in_xn("xn 11");
  std::istringstream in_sl("sl 12");
  std::istringstream in_sr("sr 13");
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_n), "Incomplete 'n' record: expected count and value", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_a), "Incomplete 'a' record: expected count and value", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_o), "Incomplete 'o' record: expected count and value", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_xr), "Incomplete 'xr' record: expected count and value", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_xn), "Incomplete 'xn' record: expected count and value", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_sl), "Incomplete 'sl' record: expected count and value", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_sr), "Incomplete 'sr' record: expected count and value", std::runtime_error);
}

TEST_CASE("parse_sequence_from_stream rejects malformed stage 4 records") {
  std::istringstream in_store("store 2");
  std::istringstream in_store_op("store 2 t 0b1 0b1");
  std::istringstream in_r("r 5");
  std::istringstream in_pr("pr");
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_store), "Incomplete 'store' record: expected slot and regular-element token", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_store_op), "Unknown regular sequence token in 'store': 't'", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_r), "Incomplete 'r' record: expected repetitions and length", std::runtime_error);
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in_pr), "Incomplete 'pr' record: expected count", std::runtime_error);
}

TEST_CASE("parse_sequence_from_stream rejects truncated explicit final terminator") {
  std::istringstream in("final");
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in), "Incomplete 'final' record: expected value", std::runtime_error);
}

TEST_CASE("parse_sequence_from_stream rejects truncated final trigger records") {
  std::istringstream in("t 0b01");
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in), "Incomplete 't' record: expected pattern and mask", std::runtime_error);
}

TEST_CASE("parse_sequence_from_stream rejects truncated non-final trigger records") {
  std::istringstream in("tn 0b01");
  CHECK_THROWS_WITH_AS(parse_sequence_from_stream(in), "Incomplete 'tn' record: expected pattern and mask", std::runtime_error);
}

TEST_CASE("spi sequence builder quantizes requested frequency") {
  spi::Config cfg;
  cfg.decoder_clock_hz = 100e6;
  cfg.spi_clock_hz = 12e6;

  spi::SequenceBuilder builder(cfg);
  builder.write_transaction({0x80});

  CHECK(builder.half_period_ticks() == 4);
  CHECK(builder.achieved_spi_clock_hz() == doctest::Approx(12.5e6));
  CHECK(builder.sequence().size() > 4);
  CHECK(builder.sequence()[0] == el(1, 0x1));
}

TEST_CASE("PMOD DA3 voltage conversion clips to DAC range") {
  CHECK(pmod_da3::code_from_voltage(-1.0) == 0x0000);
  CHECK(pmod_da3::code_from_voltage(0.0) == 0x0000);
  CHECK(pmod_da3::code_from_voltage(2.5) == 0xFFFF);
  CHECK(pmod_da3::code_from_voltage(3.0) == 0xFFFF);
  CHECK(pmod_da3::code_from_voltage(1.25) == 0x8000);
}

TEST_CASE("PMOD DA3 transaction matches spi.cc example") {
  auto builder = pmod_da3::transaction_for_voltage(2.5);

  CHECK(builder.half_period_ticks() == 5);
  CHECK(builder.achieved_spi_clock_hz() == doctest::Approx(10e6));

  Sequence expected;
  expected.push_back(el(1, 0x1));
  expected.push_back(el(2, 0x0));
  for (size_t i = 0; i < 16; i++) {
    expected.push_back(el(5, 0x2));
    expected.push_back(el(5, 0xa));
  }
  expected.push_back(el(2, 0x2));
  expected.push_back(el(4, 0x3));

  CHECK(builder.sequence() == expected);
}

TEST_CASE("write_sequence_to_stream round-trips PMOD DA3 sequence") {
  auto builder = pmod_da3::transaction_for_voltage(2.5);
  std::ostringstream out;
  write_sequence_to_stream(builder.sequence(), out, true);

  const std::string serialized = out.str();
  CHECK(contains(serialized, "d 1 0x1"));
  CHECK(contains(serialized, "f\n"));

  std::istringstream in(serialized);
  auto [seq, force_trigger] = parse_sequence_from_stream(in);
  CHECK(force_trigger);
  CHECK(seq == builder.sequence());
}

TEST_CASE("write_sequence_to_stream round-trips triggers and control-flow elements") {
  Sequence seq;
  seq.push_back(el(0x01, 0x03, false));
  seq.push_back(el(5, 0x12).store(2));
  seq.push_back(el(Replay{}, 7, 4));
  seq.push_back(el(Retrig{}));
  seq.push_back(el(PseudoRandom{}, 9));
  seq.push_back(el(0x02, 0x03, true));
  seq.push_back(el(0x34));

  std::ostringstream out;
  write_sequence_to_stream(seq, out, false);

  std::istringstream in(out.str());
  auto [roundtrip, force_trigger] = parse_sequence_from_stream(in);

  CHECK(!force_trigger);
  CHECK(compare(roundtrip, seq));
}
