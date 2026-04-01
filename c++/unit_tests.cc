// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Unit test infrastructure. This one runs on host (not on target), i.e., it is *not* cross-compiled.
// The purpose is to test very low-level (basic infrastructure) functions and classes.
// https://github.com/doctest/doctest
// https://github.com/doctest/doctest/blob/master/doc/markdown/tutorial.md

#include <iostream>
#include <sstream>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "include/doctest.h"

#include "elements.hh"
#include "sequence.hh"
#include "streamer.hh"
#include "vcd_parser.hh"

count_t get_count(const Counter &x) { return x.count(); }
control_t get_control_bits(const Counter &x) { return x.control_bits(); }

bool contains(const std::string &str, const std::string &substr) {
  return str.find(substr) != std::string::npos;
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

TEST_CASE("parse_sequence_from_stream parses existing grammar") {
  std::istringstream in("d 3 0x12 t 0b01 0b11 f d 4 0x34");
  auto [seq, force_trigger] = parse_sequence_from_stream(in);

  REQUIRE(seq.size() == 3);
  CHECK(force_trigger);

  CHECK(seq[0] == el(3, 0x12));
  CHECK(seq[1] == el(0b01, 0b11, true));
  CHECK(seq[2] == el(4, 0x34));
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
