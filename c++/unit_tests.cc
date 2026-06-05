// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Unit test infrastructure. This one runs on host (not on target), i.e., it is *not* cross-compiled.
// The purpose is to test very low-level (basic infrastructure) functions and classes.
// https://github.com/doctest/doctest
// https://github.com/doctest/doctest/blob/master/doc/markdown/tutorial.md

#include <fstream>
#include <chrono>
#include <iostream>
#include <memory>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "include/doctest.h"

#include "elements.hh"
#include "freq_meter.hh"
#include "PMODDA3.hh"
#include "SPI.hh"
#include "options.hh"
#include "ppfg.hh"
#include "pll_calc.hh"
#include "pll_rules.hh"
#include "ppwebgui_frontend.hh"
#include "ppwebgui_http.hh"
#include "ppwebgui_service_api.hh"
#include "ppworkflow.hh"
#include "sequence_file_format.hh"
#include "sequence.hh"
#include "stall_timeout.hh"
#include "streamer.hh"
#include "third_party/httplib.h"
#include "vcd_parser.hh"

// Keep host unit tests as a single translation unit. These route-only implementations are
// host-safe and let `unit_tests` exercise HTTP validation paths without linking extra objects.
#include "ppwebgui_json.cc"
#include "ppwebgui_http.cc"

count_t get_count(const Counter &x) { return x.count(); }
control_t get_control_bits(const Counter &x) { return x.control_bits(); }

bool contains(const std::string &str, const std::string &substr) {
  return str.find(substr) != std::string::npos;
}

struct ScopedStreamCapture {
  std::ostream &stream;
  std::streambuf *original = nullptr;
  std::ostringstream captured;

  explicit ScopedStreamCapture(std::ostream &target) :
    stream(target),
    original(target.rdbuf(captured.rdbuf())) {}

  ~ScopedStreamCapture() {
    stream.rdbuf(original);
  }

  std::string str() const {
    return captured.str();
  }
};

InputParser make_input(std::initializer_list<std::string> args) {
  return InputParser(std::vector<std::string>(args));
}

std::pair<Sequence, bool> roundtrip_binary(const Sequence &seq, const bool force_trigger = false) {
  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, force_trigger);
  std::istringstream in(out.str(), std::ios::binary);
  return Sequence::read_binary(in);
}

struct FakeWebGuiService : WebGuiService {
  StatusSnapshot status;
  std::string last_error;
  int apply_clock_calls = 0;
  int stream_calls = 0;
  std::function<void(const ClockConfigRequest &)> apply_clock_hook = [](const ClockConfigRequest &) {};
  std::function<StreamResult(StreamLaunchRequest)> stream_hook = [](StreamLaunchRequest) {
    return StreamResult{true, RC_OK, 200, "ok"};
  };

  StatusSnapshot get_status_copy() override { return status; }
  void apply_clock_config(const ClockConfigRequest &request) override {
    apply_clock_calls++;
    apply_clock_hook(request);
  }
  void measure_clocks() override {}
  void apply_streamer_override(const StreamerOverrideState &) override {}
  void apply_combiner_config(const CombinerRequest &) override {}
  void apply_trigger_config(const TriggerConfigRequest &) override {}
  ResetResult reset_hardware() override { return {true, "reset"}; }
  StreamResult stream_text_sequence(StreamLaunchRequest request) override {
    stream_calls++;
    return stream_hook(std::move(request));
  }
  void set_last_error(const std::string &message) override { last_error = message; }
};

struct ScopedTestWebGuiServer {
  httplib::Server server;
  std::thread thread;
  int port = -1;

  explicit ScopedTestWebGuiServer(WebGuiService &service) {
    register_ppwebgui_routes(server, service, {}, {"<html></html>", "", ""});
    port = server.bind_to_any_port("127.0.0.1");
    if (port <= 0)
      throw std::runtime_error("Failed to bind test ppwebgui server");
    thread = std::thread([this]() {
      server.listen_after_bind();
    });
  }

  ~ScopedTestWebGuiServer() {
    server.stop();
    if (thread.joinable())
      thread.join();
  }
};

struct FakeTransportControl {
  void status_report() {}
};

struct ThrowingTransport {
  void report() {}
  void send_sequence(const Sequence &) {
    throw StallTimeout("synthetic transport timeout");
  }
};

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

TEST_CASE("parse_uint helpers reject malformed input") {
  CHECK(parse_uint8_t("255") == 255);
  CHECK(parse_uint32_t("0x10") == 16);
  CHECK(parse_uint64_t("0b10101") == 21);
  CHECK_THROWS_AS(parse_uint32_t(""), std::runtime_error);
  CHECK_THROWS_AS(parse_uint32_t("abc"), std::runtime_error);
  CHECK_THROWS_AS(parse_uint8_t("256"), std::runtime_error);
  CHECK_THROWS_AS(parse_uint64_t("-1"), std::runtime_error);
}

TEST_CASE("InputParser reports missing arguments and handles first_arg_int safely") {
  const auto missing = make_input({"-port"});
  CHECK_THROWS_AS(missing.get_string("-port", "4242"), std::runtime_error);

  const auto numeric = make_input({"123"});
  REQUIRE(numeric.first_arg_int().has_value());
  CHECK(*numeric.first_arg_int() == 123);

  const auto empty = make_input({""});
  CHECK_FALSE(empty.first_arg_int().has_value());

  const auto non_numeric = make_input({"abc"});
  CHECK_FALSE(non_numeric.first_arg_int().has_value());
}

TEST_CASE("read_stable_u64 retries across a rollover") {
  int hi_reads = 0;
  const auto read_low = [&]() -> uint32_t {
    return 0;
  };
  const auto read_high = [&]() -> uint32_t {
    ++hi_reads;
    return hi_reads == 1 ? 7u : 8u;
  };

  CHECK(read_stable_u64(read_low, read_high) == (uint64_t(8) << 32));
  CHECK(hi_reads >= 3);
}

TEST_CASE("runs_counter reports n/a for empty statistics") {
  runs_counter runs([](uint32_t half, uint32_t addr) -> uint32_t {
    if (half != 0)
      return 0;
    switch (addr) {
    case 0: return 5;  // ctr_run
    case 2: return 0;  // nr_run_l
    case 3: return 4;  // nr_run_h
    case 4: return 0;  // sum_run_l
    case 5: return 20; // sum_run_h
    case 6: return 0;
    case 7: return 7;
    case 8: return 0;
    case 9: return 0;
    default: return 0;
    }
  });

  const auto report = runs.str();
  CHECK(contains(report, "avg_l=n/a"));
  CHECK(contains(report, "avg_h=5"));
}

TEST_CASE("packet_stats uses completed packets for averages") {
  packet_stats stats([](uint32_t half, uint32_t addr) -> uint32_t {
    if (half != 0)
      return 0;
    switch (addr) {
    case 0: return 30; // total
    case 1: return 20; // valid
    case 2: return 10; // idle
    case 3: return 3;  // pkt_begin
    case 4: return 2;  // pkt_end
    case 5: return 10; // pkt_len_sum
    case 6: return 52; // pkt_len_sum2
    default: return 0;
    }
  });

  const auto report = stats.str();
  CHECK(contains(report, "avg_len=5"));
}

TEST_CASE("chars_to_uint32 preserves high-bit bytes") {
  const char bytes[4] = {char(0x80), char(0xFF), char(0x01), char(0x02)};
  CHECK(chars_to_uint32(bytes) == 0x80FF0102u);
}

TEST_CASE("servo_pwm_params returns duty in percent") {
  const auto [freq_min, duty_min] = servo_pwm_params(0.0);
  const auto [freq_mid, duty_mid] = servo_pwm_params(90.0);
  const auto [freq_max, duty_max] = servo_pwm_params(180.0);

  CHECK(freq_min == doctest::Approx(50.0));
  CHECK(freq_mid == doctest::Approx(50.0));
  CHECK(freq_max == doctest::Approx(50.0));
  CHECK(duty_min == doctest::Approx(5.0));
  CHECK(duty_mid == doctest::Approx(7.5));
  CHECK(duty_max == doctest::Approx(10.0));
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

TEST_CASE("BitNot class") {
  value_t v = 42;
  BitNot v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITNOT);
  CHECK(get_mode_bits(v1) == BITNOT);
  CHECK(v1.desc() == bitnotstring);
  CHECK(get_desc(v1) == bitnotstring);
  CHECK(v1.result(0x00) == value_t(~value_t(0x00)));
  CHECK(v1.result(0x0f) == value_t(~value_t(0x0f)));
  CHECK(get_result(v1, 0x00) == value_t(~value_t(0x00)));
  CHECK(get_result(v1, 0x0f) == value_t(~value_t(0x0f)));
}

TEST_CASE("BitAnd class") {
  value_t v = 0x0c;
  BitAnd v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITAND);
  CHECK(get_mode_bits(v1) == BITAND);
  CHECK(v1.desc() == bitandstring);
  CHECK(get_desc(v1) == bitandstring);
  CHECK(v1.result(0x0a) == 0x08);
  CHECK(get_result(v1, 0x0a) == 0x08);
}

TEST_CASE("BitOr class") {
  value_t v = 0x0c;
  BitOr v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITOR);
  CHECK(get_mode_bits(v1) == BITOR);
  CHECK(v1.desc() == bitorstring);
  CHECK(get_desc(v1) == bitorstring);
  CHECK(v1.result(0x03) == 0x0f);
  CHECK(get_result(v1, 0x03) == 0x0f);
}

TEST_CASE("BitXor class") {
  value_t v = 0x0c;
  BitXor v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITXOR);
  CHECK(get_mode_bits(v1) == BITXOR);
  CHECK(v1.desc() == bitxorstring);
  CHECK(get_desc(v1) == bitxorstring);
  CHECK(v1.result(0x0a) == 0x06);
  CHECK(get_result(v1, 0x0a) == 0x06);
}

TEST_CASE("BitXnor class") {
  value_t v = 0x0c;
  BitXnor v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITXNOR);
  CHECK(get_mode_bits(v1) == BITXNOR);
  CHECK(v1.desc() == bitxnorstring);
  CHECK(get_desc(v1) == bitxnorstring);
  CHECK(v1.result(0x0a) == value_t(~value_t(0x06)));
  CHECK(get_result(v1, 0x0a) == value_t(~value_t(0x06)));
}

TEST_CASE("BitSll class") {
  value_t v = 3;
  BitSll v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITSLL);
  CHECK(get_mode_bits(v1) == BITSLL);
  CHECK(v1.desc() == bitsllstring);
  CHECK(get_desc(v1) == bitsllstring);
  CHECK(v1.result(0x11) == 0x88);
  CHECK(get_result(v1, 0x11) == 0x88);
}

TEST_CASE("BitSrl class") {
  value_t v = 3;
  BitSrl v1(v);
  CHECK(v1.value() == v);
  CHECK(get_value(v1) == v);
  CHECK(v1.mode_bits() == BITSRL);
  CHECK(get_mode_bits(v1) == BITSRL);
  CHECK(v1.desc() == bitsrlstring);
  CHECK(get_desc(v1) == bitsrlstring);
  CHECK(v1.result(0x80) == 0x10);
  CHECK(get_result(v1, 0x80) == 0x10);
}

TEST_CASE("el constructor from Counter and plain Value normalizes to bitload") {
  count_t c = 10;
  value_t v = 42;
  Counter c1(c);
  Value v1(v);
  el e(c1, v1);
  CHECK(e.count() == c);
  CHECK(e.value() == v);
  CHECK(e.control() == BITLOAD);
  CHECK(e.mode() == BITLOAD);
  CHECK(contains(e.desc(), bitloadstring));
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

TEST_CASE("replay constructor") {
  el e(Replay{}, 7, 4);
  CHECK(e.count() == 7);
  CHECK(e.value() == 4);
  CHECK(e.control() == REPLAY);
  CHECK(!e.is_regular());
  CHECK(!e.is_final());
  CHECK(contains(e.desc(), "Replay: repetitions=7 length=4"));
}

TEST_CASE("retrig constructor") {
  el e(Retrig{});
  CHECK(e.count() == 1);
  CHECK(e.value() == default_final_value);
  CHECK(e.control() == RETRIG);
  CHECK(!e.is_regular());
  CHECK(!e.is_final());
  CHECK(contains(e.desc(), retrigstring));
}

TEST_CASE("pseudo-random constructor") {
  el e(PseudoRandom{}, 9);
  CHECK(e.count() == 9);
  CHECK(e.value() == 0);
  CHECK(e.control() == PRNG);
  CHECK(!e.is_regular());
  CHECK(!e.is_final());
  CHECK(contains(e.desc(), prngstring));
}

TEST_CASE("store marks an element for replay memory") {
  el e(3, 0x12);
  e.store(2);
  CHECK(e.control() == control_t(STORE + (2u << SHIFT_POSITION)));
  CHECK(contains(e.decode(), "store 2"));
  CHECK(e != el(3, 0x12));

  const auto immutable = el(3, 0x12).stored_in(2);
  CHECK(immutable == e);
}

TEST_CASE("store rejects out-of-range slot") {
  el e(3, 0x12);
  CHECK_THROWS_WITH_AS(e.store(POSITIONS), "store() out of bounds", std::runtime_error);
  CHECK_THROWS_WITH_AS(el(3, 0x12).stored_in(POSITIONS), "store() out of bounds", std::runtime_error);
}

TEST_CASE("set_control resynchronizes regular mode semantics") {
  el e(3, BitFlip(0x03));
  e.set_control(BITSET);

  CHECK(e.control() == BITSET);
  CHECK(e.updated_value(0x04) == 0x07);
  CHECK(contains(e.desc(), bitsetstring));
}

TEST_CASE("set_control normalizes regular mode semantics for BITLOAD control") {
  el e(3, BitFlip(0x03));
  e.set_control(BITLOAD);

  CHECK(e.control() == BITLOAD);
  CHECK(e.updated_value(0x04) == 0x03);
  CHECK(contains(e.desc(), bitloadstring));
}

TEST_CASE("set_control updates special element type when control is unambiguous") {
  el e(3, 0x12);
  e.set_control(REPLAY);

  CHECK(e.is_replay());
  CHECK(!e.is_regular());
  CHECK(contains(e.desc(), "Replay: repetitions=3 length=18"));
}

TEST_CASE("set_count with counter wrapper updates count and strobe semantics") {
  el e(3, 0x12);
  e.set_count(NoStrobe(7));

  CHECK(e.count() == 7);
  CHECK((e.control() & NOSTROBE) == NOSTROBE);
  CHECK(contains(e.desc(), nostrobestring));
}

TEST_CASE("set_value updates regular mode semantics") {
  el e(3, 0x12);
  e.set_value(BitXor(0x03));

  CHECK((e.control() & MODEBITS) == BITXOR);
  CHECK(e.updated_value(0x04) == 0x07);
  CHECK(contains(e.desc(), bitxorstring));
}

TEST_CASE("immutable el transforms preserve regular semantics") {
  SUBCASE("with_count") {
    const el original(NoStrobe(3), BitSet(0x12));
    const auto updated = original.with_count(7);

    CHECK(original.count() == 3);
    CHECK(updated.count() == 7);
    CHECK(updated.no_strobe());
    CHECK(updated.mode() == BITSET);
    CHECK(updated.updated_value(0x01) == 0x13);
  }

  SUBCASE("with_counter") {
    const el original(3, BitSet(0x12));
    const auto updated = original.with_counter(NoStrobe(7));

    CHECK(original.count() == 3);
    CHECK(updated.count() == 7);
    CHECK(updated.no_strobe());
    CHECK(contains(updated.desc(), nostrobestring));
  }

  SUBCASE("with_regular_value") {
    const el original(NoStrobe(3), BitSet(0x12));
    const auto updated = original.with_regular_value(BitXor(0x03));

    CHECK(updated.count() == 3);
    CHECK(updated.no_strobe());
    CHECK(updated.mode() == BITXOR);
    CHECK(updated.updated_value(0x04) == 0x07);
    CHECK(contains(updated.desc(), bitxorstring));
  }

  SUBCASE("as_bitload_after") {
    const el original(NoStrobe(3), BitFlip(0x03));
    const auto updated = original.as_bitload_after(0x04);

    CHECK(updated.no_strobe());
    CHECK(updated.mode() == BITLOAD);
    CHECK(updated.value() == 0x07);
    CHECK(updated.updated_value(0x00) == 0x07);
    CHECK(contains(updated.desc(), bitloadstring));
  }
}

TEST_CASE("element helpers decode store and trigger fields") {
  SUBCASE("store helpers") {
    el e(3, 0x12);
    CHECK_FALSE(e.is_stored());
    CHECK_THROWS_WITH_AS(e.store_slot(), "Element is not marked for storage", std::runtime_error);

    e.store(2);
    CHECK(e.is_stored());
    CHECK(e.store_slot() == 2);
  }

  SUBCASE("trigger helpers") {
    el e(0x55, 0x2a, true);
    CHECK(e.trigger_pattern() == 0x55);
    CHECK(e.trigger_mask() == 0x2a);
    CHECK(e.trigger_is_final());
  }
}

TEST_CASE("classify_control matches element control categories") {
  CHECK(el::classify_control(BITLOAD) == el_type::regular);
  CHECK(el::classify_control(TRIGGER) == el_type::trigger);
  CHECK(el::classify_control(REPLAY) == el_type::replay);
  CHECK(el::classify_control(RETRIG) == el_type::retrig);
  CHECK(el::classify_control(PRNG) == el_type::prng);
  CHECK(el::classify_control(TERMINATE) == el_type::final);
}

TEST_CASE("static control and trigger decoders match encoded values") {
  CHECK(el::mode_from_control(BITXOR | NOSTROBE) == BITXOR);
  CHECK(el::no_strobe_from_control(BITXOR | NOSTROBE));
  CHECK(el::stored_from_control(control_t(STORE | (3u << SHIFT_POSITION))));
  CHECK(el::store_slot_from_control(control_t(STORE | (3u << SHIFT_POSITION))) == 3);
  CHECK_THROWS_WITH_AS(el::store_slot_from_control(BITLOAD), "Element is not marked for storage", std::runtime_error);

  const value_t trigger_value = (value_t(0x2a) << WIDTH_TRIGGER) | value_t(0x55);
  CHECK(el::trigger_pattern_from_value(trigger_value) == 0x55);
  CHECK(el::trigger_mask_from_value(trigger_value) == 0x2a);
  CHECK(el::trigger_final_from_control(TRIGGER | TRIGGERFINAL));
}

TEST_CASE("from_raw_triplet reconstructs encoded elements") {
  SUBCASE("regular") {
    const auto e = el::from_raw_triplet(BITSET | NOSTROBE, 7, 0x12);
    CHECK(e.is_regular());
    CHECK(e.count() == 7);
    CHECK(e.value() == 0x12);
    CHECK(e.no_strobe());
    CHECK(e.mode() == BITSET);
    CHECK(e.updated_value(0x01) == 0x13);
  }

  SUBCASE("replay") {
    const auto e = el::from_raw_triplet(REPLAY, 3, 9);
    CHECK(e.is_replay());
    CHECK(e.count() == 3);
    CHECK(e.value() == 9);
  }

  SUBCASE("trigger") {
    const auto trigger_value = (value_t(0x2a) << WIDTH_TRIGGER) | value_t(0x55);
    const auto e = el::from_raw_triplet(TRIGGER | TRIGGERFINAL, 0, trigger_value);
    CHECK(e.is_trigger());
    CHECK(e.trigger_pattern() == 0x55);
    CHECK(e.trigger_mask() == 0x2a);
    CHECK(e.trigger_is_final());
  }
}

TEST_CASE("regular token helpers round-trip regular element encoding") {
  CHECK(el::is_regular_token("xr"));
  CHECK(el::is_regular_token("dn"));
  CHECK_FALSE(el::is_regular_token("t"));

  const auto e = el::from_regular_token("xr", 7, 0x12);
  CHECK(e.is_regular());
  CHECK(e.count() == 7);
  CHECK(e.value() == 0x12);
  CHECK(e.mode() == BITXOR);
  CHECK(e.regular_token() == "xr");

  const auto dn = el::from_regular_token("dn", 3, 0x55);
  CHECK(dn.no_strobe());
  CHECK(dn.regular_token() == "dn");
}

TEST_CASE("sequence_record serializes element kinds") {
  CHECK(el(7, BitXor(0x12)).sequence_record() == "xr 7 0x12");
  CHECK(el(7, BitXor(0x12)).stored_in(3).sequence_record() == "store 3 xr 7 0x12");
  CHECK(el(0x55, 0x2a, true).sequence_record() == "t 0x55 0x2a");
  CHECK(el(Replay{}, 3, 9).sequence_record() == "r 3 0x9");
  CHECK(el(Retrig{}).sequence_record() == "rt");
  CHECK(el(PseudoRandom{}, 11).sequence_record() == "pr 11");
  CHECK(el(0x42).sequence_record() == "final 0x42");
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

TEST_CASE("Sequence length and data_size count only regular elements") {
  Sequence seq;
  seq.push_back(el(0x01, 0x03, false));
  seq.push_back(el(3, 0x12));
  seq.push_back(el(Replay{}, 7, 4));
  seq.push_back(el(NoStrobe(5), 0x34));
  seq.push_back(el(0x56));

  CHECK(seq.length() == 8);
  CHECK(seq.data_size() == 2);
}

TEST_CASE("merge preserves regular control semantics") {
  Sequence seq;
  seq.push_back(el(NoStrobe(2), BitSet(0x03)).store(1));
  seq.push_back(el(NoStrobe(3), BitSet(0x03)).store(1));

  Sequence expected;
  el merged(NoStrobe(5), BitSet(0x03));
  merged.store(1);
  expected.push_back(merged);

  const auto result = seq.merge();
  REQUIRE(result.size() == 1);
  CHECK(result == expected);
  CHECK(result[0].updated_value(0x01) == expected[0].updated_value(0x01));
  CHECK(result[0].desc() == expected[0].desc());
}

TEST_CASE("merge keeps adjacent regular elements with different control separate") {
  Sequence seq;
  seq.push_back(el(2, 0x12));
  seq.push_back(el(NoStrobe(3), 0x12));
  seq.push_back(el(4, BitSet(0x12)));

  CHECK(seq.merge() == seq);
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

TEST_CASE("pll calculator finds strict exact 66 MHz solution") {
  auto params = pllcalc::calculate("66M");

  REQUIRE(params.has_value());
  CHECK(params->n == 5);
  CHECK(params->m == 99);
  CHECK(params->c == 15);
  CHECK(params->actual_hz == doctest::Approx(66.0e6));
  CHECK(params->pfd_hz >= pllcalc::pfd_min_hz);
  CHECK(params->pfd_hz <= pllcalc::pfd_max_hz);
  CHECK(params->vco_hz >= pllcalc::vco_min_hz);
  CHECK(params->vco_hz <= pllcalc::vco_max_hz);
}

TEST_CASE("pll calculator rejects unreachable strict low frequency") {
  CHECK(!pllcalc::calculate("10k").has_value());
}

TEST_CASE("pll profile resolution preserves presets and raw strings") {
  auto preset = pllcalc::resolve_profile("100M", applyReplacement("100M", pll_rules));
  CHECK(preset.config == "5,20,2");
  CHECK(!preset.calculated.has_value());

  auto raw = pllcalc::resolve_profile("7,33,11", applyReplacement("7,33,11", pll_rules));
  CHECK(raw.config == "7,33,11");
  CHECK(!raw.calculated.has_value());
}

TEST_CASE("pll profile resolution calculates unknown frequency strings") {
  auto resolved = pllcalc::resolve_profile("66M", applyReplacement("66M", pll_rules));

  CHECK(resolved.config == "5,99,15");
  REQUIRE(resolved.calculated.has_value());
  CHECK(resolved.calculated->actual_hz == doctest::Approx(66.0e6));
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

TEST_CASE("parse_sequence_file_format accepts supported names") {
  CHECK(parse_sequence_file_format("vcd") == SequenceFileFormat::vcd);
  CHECK(parse_sequence_file_format("text") == SequenceFileFormat::text);
  CHECK(parse_sequence_file_format("binary") == SequenceFileFormat::binary);
}

TEST_CASE("parse_sequence_file_format rejects unknown names") {
  CHECK_THROWS_AS(parse_sequence_file_format("txt"), std::runtime_error);
  CHECK_THROWS_AS(parse_sequence_file_format("foo"), std::runtime_error);
}

TEST_CASE("infer_sequence_file_format_from_filename infers known extensions") {
  CHECK(infer_sequence_file_format_from_filename("waveform.vcd") == SequenceFileFormat::vcd);
  CHECK(infer_sequence_file_format_from_filename("capture.seq") == SequenceFileFormat::text);
  CHECK(infer_sequence_file_format_from_filename("capture.txt") == SequenceFileFormat::text);
  CHECK(infer_sequence_file_format_from_filename("capture.bin") == SequenceFileFormat::binary);
  CHECK(infer_sequence_file_format_from_filename("capture.ppbin") == SequenceFileFormat::binary);
}

TEST_CASE("resolve_sequence_file_format prefers explicit format") {
  auto input = make_input({"-format", "text"});
  CHECK(resolve_sequence_file_format(input, "capture.vcd") == SequenceFileFormat::text);
}

TEST_CASE("resolve_sequence_file_format uses forced default") {
  auto input = make_input({});
  CHECK(resolve_sequence_file_format(input, "capture.unknown", SequenceFileFormat::vcd) == SequenceFileFormat::vcd);
}

TEST_CASE("resolve_sequence_file_format rejects ambiguous filename") {
  auto input = make_input({});
  CHECK_THROWS_AS(resolve_sequence_file_format(input, "capture"), std::runtime_error);
}

TEST_CASE("validate_sequence_file_options enforces VCD-only options") {
  CHECK_NOTHROW(validate_sequence_file_options(make_input({"-target", "outs", "-scale", "10"}), SequenceFileFormat::vcd));
  CHECK_THROWS_AS(validate_sequence_file_options(make_input({"-target", "outs"}), SequenceFileFormat::text), std::runtime_error);
  CHECK_THROWS_AS(validate_sequence_file_options(make_input({"-scale", "10"}), SequenceFileFormat::text), std::runtime_error);
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

TEST_CASE("append_final_output reuses authored final terminator") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  seq.push_back(el(0x34));

  auto final = append_final_output(seq, make_input({}));

  CHECK(final == 0x34);
  REQUIRE(seq.size() == 2);
  CHECK(seq.back() == el(0x34));
}

TEST_CASE("append_final_output rejects -t when sequence already has final terminator") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  seq.push_back(el(0x34));

  CHECK_THROWS_WITH_AS(
    append_final_output(seq, make_input({"-t", "0x56"})),
    "Sequence already contains an explicit final output; omit -t or remove the final record",
    std::runtime_error);
}

TEST_CASE("prepare_sequence_for_streaming keeps caller sequence unchanged") {
  Sequence seq;
  seq.push_back(el(3, 0x12));

  auto [prepared, final] = prepare_sequence_for_streaming(seq, make_input({"-t", "0x34"}));

  CHECK(final == 0x34);
  REQUIRE(seq.size() == 1);
  CHECK(seq.back() == el(3, 0x12));
  REQUIRE(prepared.size() == 2);
  CHECK(prepared.back() == el(0x34));
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

TEST_CASE("readback_timeout_policy uses safe defaults when timeout is omitted") {
  ScopedStreamCapture capture(std::cout);
  const auto policy = readback_timeout_policy(make_input({}));

  CHECK(policy.first_element_timeout_s == doctest::Approx(default_readback_first_element_timeout_s));
  CHECK(policy.idle_timeout_s == doctest::Approx(default_readback_idle_timeout_s));
  CHECK(policy.total_timeout_s == doctest::Approx(0.0));
  CHECK(capture.str().empty());
}

TEST_CASE("readback_timeout_policy respects explicit timeout overrides") {
  ScopedStreamCapture capture(std::cout);
  const auto idle_policy = readback_timeout_policy(make_input({"-timeout", "0.25"}));
  CHECK(idle_policy.first_element_timeout_s == doctest::Approx(0.0));
  CHECK(idle_policy.idle_timeout_s == doctest::Approx(0.25));
  CHECK(idle_policy.total_timeout_s == doctest::Approx(0.0));

  const auto disabled_policy = readback_timeout_policy(make_input({"-timeout", "0"}));
  CHECK_FALSE(disabled_policy.enabled());

  const auto total_policy = readback_timeout_policy(make_input({"-timeout", "-1.5"}));
  CHECK(total_policy.first_element_timeout_s == doctest::Approx(0.0));
  CHECK(total_policy.idle_timeout_s == doctest::Approx(0.0));
  CHECK(total_policy.total_timeout_s == doctest::Approx(1.5));

  const auto output = capture.str();
  CHECK(contains(output, "readback timeout=0.25s [after last read]"));
  CHECK(contains(output, "readback timeout disabled"));
  CHECK(contains(output, "readback timeout=1.5s [after start]"));
}

TEST_CASE("freq_meter normalizes zero-length gate requests") {
  CHECK(freq_meter::normalize_gate_len(0) == 1);
  CHECK(freq_meter::normalize_gate_len(7) == 7);
}

TEST_CASE("freq_meter waits at least one microsecond for tiny gates") {
  CHECK(freq_meter::gate_wait_time_us(1, 50'000'000.0) == 1);
  CHECK(freq_meter::gate_wait_time_us(500'000, 50'000'000.0) == 10'000);
}

TEST_CASE("TimeoutGuard raises total timeout after the deadline") {
  TimeoutGuard guard("unit-test total timeout", 0.02);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  CHECK_THROWS_AS(guard.throw_if_total_timeout("details"), StallTimeout);
}

TEST_CASE("TimeoutGuard resets the stall deadline on progress") {
  TimeoutGuard guard("unit-test stall timeout", 0.02);
  std::optional<int> previous;

  CHECK(guard.progress_if_changed(previous, 1));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  CHECK_NOTHROW(guard.throw_if_stalled("before progress update"));
  CHECK(guard.progress_if_changed(previous, 2));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  CHECK_NOTHROW(guard.throw_if_stalled("after progress update"));
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
  CHECK_THROWS_AS(guard.throw_if_stalled("after idle wait"), StallTimeout);
}

TEST_CASE("TimeoutGuard can be disabled") {
  TimeoutGuard guard("unit-test disabled timeout", 0.0);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  CHECK_NOTHROW(guard.throw_if_total_timeout("disabled"));
  CHECK_NOTHROW(guard.throw_if_stalled("disabled"));
}

TEST_CASE("transmit_sequence_checked maps transport timeout to RC_TIMEOUT") {
  ThrowingTransport transport;
  FakeTransportControl control;
  Sequence seq;
  Verbosity verbosity;
  std::ostringstream out;

  const auto rc = transmit_sequence_checked(transport, control, seq, verbosity, out);

  CHECK(rc == RC_TIMEOUT);
  CHECK(contains(out.str(), "synthetic transport timeout"));
}

TEST_CASE("ppwebgui routes return 400 for service-side bad requests") {
  ScopedStreamCapture capture(std::cerr);
  FakeWebGuiService service;
  service.stream_hook = [](StreamLaunchRequest) -> StreamResult {
    throw WebGuiBadRequest("Sequence already contains an explicit final output; omit -t or remove the final record");
  };

  ScopedTestWebGuiServer server(service);
  httplib::Client client("127.0.0.1", server.port);
  const httplib::Params params {
    {"sequence_text", "d 1 0x1\n"},
    {"check_readback", "0"},
  };

  const auto res = client.Post("/api/stream", params);

  REQUIRE(res);
  CHECK(res->status == httplib::StatusCode::BadRequest_400);
  CHECK(contains(res->body, "explicit final output"));
  CHECK(service.stream_calls == 1);
  CHECK(service.last_error == "Sequence already contains an explicit final output; omit -t or remove the final record");
  CHECK(contains(capture.str(), "ppwebgui: HTTP 400 error: Sequence already contains an explicit final output; omit -t or remove the final record"));
}

TEST_CASE("ppwebgui routes reject malformed clock requests before calling the service") {
  ScopedStreamCapture capture(std::cerr);
  FakeWebGuiService service;

  ScopedTestWebGuiServer server(service);
  httplib::Client client("127.0.0.1", server.port);
  const httplib::Params params {
    {"core_profile", "100M"},
    {"int_profile", "100M"},
  };

  const auto res = client.Post("/api/clocking", params);

  REQUIRE(res);
  CHECK(res->status == httplib::StatusCode::BadRequest_400);
  CHECK(contains(res->body, "Missing parameter: source"));
  CHECK(service.apply_clock_calls == 0);
  CHECK(service.last_error == "Missing parameter: source");
  CHECK(contains(capture.str(), "ppwebgui: HTTP 400 error: Missing parameter: source"));
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

TEST_CASE("binary round-trips regular sequence exactly") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  seq.push_back(el(NoStrobe(4), 0x34));
  seq.push_back(el(5, BitSet(0x40)).store(2));

  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, false);
  std::istringstream in(out.str(), std::ios::binary);
  auto [roundtrip, force_trigger] = Sequence::read_binary(in);

  CHECK(!force_trigger);
  CHECK(compare(roundtrip, seq));
}

TEST_CASE("binary round-trip preserves regular element semantics") {
  Sequence seq;
  seq.push_back(el(1, BitLoad(0x12)));
  seq.push_back(el(NoStrobe(2), BitSet(0x03)).store(1));
  seq.push_back(el(3, BitClear(0x0c)));
  seq.push_back(el(4, BitFlip(0x30)));
  seq.push_back(el(5, BitNot(0xff)));
  seq.push_back(el(6, BitAnd(0x0f)));
  seq.push_back(el(7, BitOr(0xf0)));
  seq.push_back(el(8, BitXor(0x33)));
  seq.push_back(el(9, BitXnor(0x55)));
  seq.push_back(el(10, BitSll(3)));
  seq.push_back(el(11, BitSrl(2)));

  auto [roundtrip, force_trigger] = roundtrip_binary(seq);

  CHECK(!force_trigger);
  REQUIRE(roundtrip.size() == seq.size());

  value_t expected_prev = 0x12345678;
  value_t actual_prev = expected_prev;
  for (size_t i = 0; i < seq.size(); ++i) {
    CHECK(roundtrip[i] == seq[i]);
    CHECK(roundtrip[i].desc() == seq[i].desc());

    const auto expected = seq[i].updated_value(expected_prev);
    const auto actual = roundtrip[i].updated_value(actual_prev);
    CHECK(actual == expected);

    expected_prev = expected;
    actual_prev = actual;
  }
}

TEST_CASE("binary round-trips mixed sequence exactly") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  seq.push_back(el(NoStrobe(4), 0x34));
  seq.push_back(el(5, BitFlip(0x40)).store(2));
  seq.push_back(el(0x01, 0x03, false));
  seq.push_back(el(Replay{}, 7, 4));
  seq.push_back(el(Retrig{}));
  seq.push_back(el(PseudoRandom{}, 9));
  seq.push_back(el(0x02, 0x03, true));
  seq.push_back(el(0x34));

  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, true);
  std::istringstream in(out.str(), std::ios::binary);
  auto [roundtrip, force_trigger] = Sequence::read_binary(in);

  CHECK(force_trigger);
  CHECK(compare(roundtrip, seq));
}

TEST_CASE("binary round-trips empty sequence") {
  Sequence seq;

  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, true);
  std::istringstream in(out.str(), std::ios::binary);
  auto [roundtrip, force_trigger] = Sequence::read_binary(in);

  CHECK(force_trigger);
  CHECK(roundtrip.empty());
}

TEST_CASE("binary file round-trips sequence exactly") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  seq.push_back(el(NoStrobe(4), 0x34));
  seq.push_back(el(5, BitSet(0x40)).store(2));

  const std::string filename = "unit_tests_output_roundtrip.ppbin";
  seq.write_binary_file(filename, true);
  auto [roundtrip, force_trigger] = Sequence::read_binary_file(filename);

  CHECK(force_trigger);
  CHECK(compare(roundtrip, seq));
  std::remove(filename.c_str());
}

TEST_CASE("binary reader rejects bad magic") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, false);
  auto data = out.str();
  data[0] = 'X';
  std::istringstream in(data, std::ios::binary);
  CHECK_THROWS_AS(Sequence::read_binary(in), std::runtime_error);
}

TEST_CASE("binary reader rejects unsupported major version") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, false);
  auto data = out.str();
  data[4] = char(2);
  std::istringstream in(data, std::ios::binary);
  CHECK_THROWS_AS(Sequence::read_binary(in), std::runtime_error);
}

TEST_CASE("binary reader rejects unsupported endianness") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, false);
  auto data = out.str();
  data[6] = char(2);
  std::istringstream in(data, std::ios::binary);
  CHECK_THROWS_AS(Sequence::read_binary(in), std::runtime_error);
}

TEST_CASE("binary reader rejects unsupported payload kind") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, false);
  auto data = out.str();
  data[7] = char(1);
  std::istringstream in(data, std::ios::binary);
  CHECK_THROWS_AS(Sequence::read_binary(in), std::runtime_error);
}

TEST_CASE("binary reader rejects width mismatch") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, false);
  auto data = out.str();
  data[8] = char(0x10);
  data[9] = char(0x00);
  std::istringstream in(data, std::ios::binary);
  CHECK_THROWS_AS(Sequence::read_binary(in), std::runtime_error);
}

TEST_CASE("binary reader rejects unsupported header size") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, false);
  auto data = out.str();
  data[20] = char(0x00);
  data[21] = char(0x00);
  data[22] = char(0x00);
  data[23] = char(0x00);
  std::istringstream in(data, std::ios::binary);
  CHECK_THROWS_AS(Sequence::read_binary(in), std::runtime_error);
}

TEST_CASE("binary reader rejects payload size mismatch") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, false);
  auto data = out.str();
  data[32] = char(0x01);
  data[33] = char(0x00);
  data[34] = char(0x00);
  data[35] = char(0x00);
  data[36] = char(0x00);
  data[37] = char(0x00);
  data[38] = char(0x00);
  data[39] = char(0x00);
  std::istringstream in(data, std::ios::binary);
  CHECK_THROWS_AS(Sequence::read_binary(in), std::runtime_error);
}

TEST_CASE("binary reader rejects truncated payload") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, false);
  auto data = out.str();
  data.resize(data.size() - 1);
  std::istringstream in(data, std::ios::binary);
  CHECK_THROWS_AS(Sequence::read_binary(in), std::runtime_error);
}

TEST_CASE("binary reader rejects trailing data after payload") {
  Sequence seq;
  seq.push_back(el(3, 0x12));
  std::ostringstream out(std::ios::binary);
  seq.write_binary(out, false);
  auto data = out.str();
  data.push_back(char(0x00));
  std::istringstream in(data, std::ios::binary);
  CHECK_THROWS_AS(Sequence::read_binary(in), std::runtime_error);
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

TEST_CASE("write_sequence_to_stream serializes all regular operator tokens") {
  Sequence seq;
  seq.push_back(el(1, 0x1));
  seq.push_back(el(NoStrobe(2), 0x2));
  seq.push_back(el(3, BitSet(0x3)));
  seq.push_back(el(4, BitClear(0x4)));
  seq.push_back(el(5, BitFlip(0x5)));
  seq.push_back(el(6, BitNot(0x6)));
  seq.push_back(el(7, BitAnd(0x7)));
  seq.push_back(el(8, BitOr(0x8)));
  seq.push_back(el(9, BitXor(0x9)));
  seq.push_back(el(10, BitXnor(0xa)));
  seq.push_back(el(11, BitSll(0xb)));
  seq.push_back(el(12, BitSrl(0xc)));
  seq.push_back(el(13, BitSet(0xd)).store(2));

  std::ostringstream out;
  write_sequence_to_stream(seq, out, false);

  const std::string expected =
      "d 1 0x1\n"
      "dn 2 0x2\n"
      "s 3 0x3\n"
      "c 4 0x4\n"
      "x 5 0x5\n"
      "n 6 0x6\n"
      "a 7 0x7\n"
      "o 8 0x8\n"
      "xr 9 0x9\n"
      "xn 10 0xa\n"
      "sl 11 0xb\n"
      "sr 12 0xc\n"
      "store 2 s 13 0xd\n";
  CHECK(out.str() == expected);

  std::istringstream in(out.str());
  auto [roundtrip, force_trigger] = parse_sequence_from_stream(in);
  CHECK(!force_trigger);
  CHECK(roundtrip == seq);
}

TEST_CASE("write_sequence_to_stream rejects non-BITLOAD no-strobe elements") {
  Sequence seq;
  seq.push_back(el(NoStrobe(3), BitSet(0x12)));

  std::ostringstream out;
  CHECK_THROWS_WITH_AS(write_sequence_to_stream(seq, out, false),
                       "Text sequence writer does not support non-BITLOAD no-strobe elements",
                       std::runtime_error);
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
