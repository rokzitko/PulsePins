// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Streaming-oriented `pptool` command implementations.

#include <bitset>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <unistd.h>

#include "pptool_commands.hh"
#include "address_map.hh"
#include "delay.hh"
#include "ppmisc.hh"
#include "pptest.hh"

#include "ppmstest.hh"
#include "ppdmatest.hh"
#include "ppfg.hh"
#include "ppworkflow.hh"
#include "trigger.hh"

auto get_test_number(const InputParser &input)
{
  return input.first_arg_int().value_or(1);
}

int pptool([[maybe_unused]] FPGA &fpga,
           [[maybe_unused]] const InputParser &input,
           [[maybe_unused]] const Verbosity &v)
{
  std::cout << "Done." << std::endl;
  return RC_OK;
}

int pptest(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  const int test = get_test_number(input);
  int rc = RC_OK; // return code
  try {
    streamer s(input, fpga);
    trigger tr(input, fpga);
    readback rb(input, fpga);
    counter ctr(input, fpga);
    ctr.reset_all();
    tests t(fpga, s, rb, ctr, input, v);
    rc = t.run(test);
  }
  catch (const char *e) {
    std::cout << "exception: " << e << std::endl;
    rc = RC_EXCEPTION;
  }
  return rc;
}

int ppmstest(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  const int test = get_test_number(input);
  int rc = RC_OK; // return code
  try {
    multistreamer s(input, fpga);
    qout q(input, v, fpga);
    readback rb(input, fpga);
    trigger_int trig_int(fpga.dev_lw, address_map::lw::pio_trig_int);
    mstests t(s, q, rb, trig_int, input, v);
    rc = t.run(test);
  }
  catch (const char *e) {
    std::cout << "exception: " << e << std::endl;
    rc = RC_EXCEPTION;
  }
  return rc;
}

int ppdmatest(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  const int test = get_test_number(input);
  int rc = RC_OK; // return code
  try {
    dma_streamer s(input, fpga);
    readback rb(input, fpga);
    counter ctr(input, fpga);
    dmatests t(s, rb, ctr, input, v);
    rc = t.run(test);
  }
  catch (const char *e) {
    std::cout << "exception: " << e << std::endl;
    rc = RC_EXCEPTION;
  }
  return rc;
}

std::pair<trigger_t, trigger_t> get_trigger_pm(const InputParser &input, const bool verbose)
{
  const auto p = parse_trigger(input, "-p", "0b00000001");
  const auto m = parse_trigger(input, "-m", "0b00000001");
  if (verbose)
    std::cout << "Trigger: pattern=" << std::bitset<WIDTH_TRIGGER>(p)
      << " mask=" << std::bitset<WIDTH_TRIGGER>(m) << std::endl;
  return {p, m};
}

int ppfg(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  streamer s(input, fpga); // must be called first to setup the PLL

  const auto [period_req, duty] = resolve_ppfg_timing(input);
  const double output_clk = fpga.pll_int.int_clk.get_freq(0);
  if (v.verbose)
    std::cout << "output_clk=" << pretty_frequency(output_clk) << " output_clk_period="
    << pretty_time(1.0/output_clk) << std::endl;
  const auto [nr_pos, nr_neg] = calc_pos_neg(period_req, duty, output_clk);

  const auto v1 = parse_value(input, "-v1", "0xFFFFFFFF");
  const auto v0 = parse_value(input, "-v0", "0x00000000");
  const bool start0 = input.exists("-start0");

  const auto delay = parse_time(input, "-delay", "0");
  const auto nr_delay = calc_delay(delay, output_clk);

  trigger tr(input, fpga);
  const auto [p, m] = get_trigger_pm(input, v.verbose);
  const bool autotrig = input.exists("-trig") || input.exists("-autotrig");

  s.sc.set_gating_from_string(input.get_string("-gate", ""));
  const double delay_between_readings = 0.1;
  if (input.exists("-gate_debug")) {
    for (;;) {
      sleepd(delay_between_readings);
      std::cout << "Gate: " << s.sc.gate_status_string() << std::endl;
    }
  }

  if (input.exists("-cont")) {
    if (v.verbose)
      std::cout << "Continuous mode" << std::endl;
    auto seq = seq_continuous(p, m, nr_delay, nr_pos, nr_neg, v1, v0, start0);
    if (v.veryverbose)
      seq.dump(std::cout, "| ");
    int rc = transmit_sequence_checked(s.fifo, s.sc, seq, v);
    if (rc != RC_OK)
      return rc;
    if (autotrig) {
      rc = force_trigger_when_output_fifo_ready(s.sc, v);
      if (rc != RC_OK)
        return rc;
    } else {
      s.sc.trigger_enable();
    }
    for (;;)
      pause();
  }

  if (input.exists("-burst")) {
    const auto rep = input.get_uint32("-burst", 1);
    if (v.verbose)
      std::cout << "Burst mode: repetitions=" << rep << std::endl;
    const auto final = parse_value(input, "-t", "0");
    const auto seq = seq_burst(p, m, nr_delay, nr_pos, nr_neg, v1, v0, rep, start0, final);
    if (v.veryverbose)
      seq.dump(std::cout, "| ");
    auto seq_retrig = with_pushed(seq, el(Retrig{}));
    auto seq_final  = with_pushed(seq, el(final));

    const count_t n_max = parse_uint32(input, "-n_max", "1");
    int rc = RC_OK;
    for (count_t n = 0; n_max == 0 || n < n_max; n++) {
      if (n_max == 0 || n < n_max-1)
        rc |= transmit_sequence_checked(s.fifo, s.sc, seq_retrig, v);
      else
        rc |= transmit_sequence_checked(s.fifo, s.sc, seq_final, v);
      if (rc & RC_TIMEOUT)
        return rc;
      if (n == 0) {
        if (autotrig) {
          const int trigger_rc = force_trigger_when_output_fifo_ready(s.sc, v);
          rc |= trigger_rc;
          if (trigger_rc != RC_OK)
            return rc;
        } else {
          s.sc.trigger_enable();
        }
      }
    }
  }
  return RC_OK;
}

count_t calc_duration_nr(const double duration_req,
                        const bool verbose,
                        const double output_clk = default_output_clk)
{
  if (!std::isfinite(duration_req) || duration_req < 0.0)
    throw std::runtime_error("Duration must be a finite non-negative quantity.");
  if (!std::isfinite(output_clk) || output_clk <= 0.0)
    throw std::runtime_error("Output clock frequency must be a finite positive quantity.");
  const double output_clk_period = 1.0/output_clk;
  if (verbose)
    std::cout << "output_clk=" << pretty_frequency(output_clk) << " output_clk_period="
    << pretty_time(output_clk_period) << std::endl;
  double nr_d = round(duration_req/output_clk_period);
  nr_d = (nr_d > 0.0 ? nr_d : 1.0);
  if (!std::isfinite(nr_d) || nr_d > max_count_t)
    throw std::runtime_error("Duration exceeds the limit of the count_t timer range.");
  const uint64_t nr = static_cast<uint64_t>(nr_d);
  const double duration_resulting = nr*output_clk_period;
  std::cout << "duration (requested)=" << pretty_time(duration_req) << " (resulting)="
    << pretty_time(duration_resulting) << " nr=" << std::dec << nr << std::endl;
  return nr;
}

int ppdelay(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  streamer s(input, fpga);
  trigger tr(input, fpga);
  const auto [p, m] = get_trigger_pm(input, v.verbose);
  const auto delay = parse_time(input, "-delay", "0");
  const double output_clk = fpga.pll_int.int_clk.get_freq(0);
  const auto nr_delay = calc_delay(delay, output_clk);
  const auto pulse_duration = parse_time(input, "-duration", "0");
  const auto nr = calc_duration_nr(pulse_duration, v.verbose, output_clk);
  const auto nr_neg = 1;
  const auto v1 = parse_value(input, "-v1", "0xFFFFFFFF");
  const auto v0 = parse_value(input, "-v0", "0x00000000");
  const auto final = parse_value(input, "-t", "0");
  auto seq = seq_once(p, m, nr_delay, nr, nr_neg, v1, v0, final);
  if (v.veryverbose) seq.dump(std::cout, "| ");
  const auto rc = transmit_sequence_checked(s.fifo, s.sc, seq, v);
  if (rc != RC_OK)
    return rc;
  s.sc.trigger_enable();
  return RC_OK;
}

int ppreset(FPGA &fpga,
            const InputParser &input,
            [[maybe_unused]] const Verbosity &v)
{
  streamer s(input, fpga);
  return RC_OK;
}

int pptrig(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  streamer s(input, fpga);
  trigger tr(input, fpga);
  trigger_int trig_int(fpga.dev_lw, address_map::lw::pio_trig_int, false);
  auto p = parse_uint32(input, "-pio", "0");
  trig_int.write(p);
  if (v.veryverbose) {
    auto report = [](std::string s, value_t v) {
      std::cout << "tr(" << s << ")=0x" << std::hex << std::setfill('0') << std::setw(2) << (v & 0xFF) << " " << std::bitset<WIDTH_TRIGGER>(v) << std::endl;
    };
    report("int",  tr.ct.in1());
    report("ext",  tr.ct.in2());
    report("misc", tr.ct.in3());
    report("out",  tr.ct.out());
  }
  if (input.exists("-debug"))
    s.sc.monitor_ext_trig();
  return RC_OK;
}

int ppqout(FPGA &fpga, const InputParser &input, const Verbosity &verb)
{
  multistreamer s(input, fpga);
  qout q(input, verb, fpga);
  if (input.exists("-self_test")) {
    const auto self_test_rc = q.cq.self_test();
    if (self_test_rc != 0) {
      std::cout << red << "Self test failed: detail rc=" << self_test_rc << rst << std::endl;
      return RC_ERROR_CHECK;
    }
  }
  if (input.exists("-report_pre"))
    q.cq.report();
  s.s1.sc.qout_set(parse_value(input, "-q1", "0"));
  s.s2.sc.qout_set(parse_value(input, "-q2", "0"));
  s.s3.sc.qout_set(parse_value(input, "-q3", "0"));
  s.s4.sc.qout_set(parse_value(input, "-q4", "0"));
  auto report = [](const std::string s, const value_t v) {
    std::cout << "qout(" << s << ")=0x" << std::hex << std::setfill('0') << v << " " << std::bitset<WIDTH_DATA>(v) << std::endl;
  };
  if (verb.veryverbose) {
    report("streamer 1", s.s1.sc.get_qout());
    report("streamer 2", s.s2.sc.get_qout());
    report("streamer 3", s.s3.sc.get_qout());
    report("streamer 4", s.s4.sc.get_qout());
  }
  report("combiner_in1", q.in1());
  report("combiner_in2", q.in2());
  report("combiner_in3", q.in3());
  report("combiner_in4", q.in4());
  report("combiner_out", q.out());
  if (input.exists("-report_post"))
    q.cq.report();
  if (input.exists("-test")) {
    const int nr = input.get_uint32("-test", 10000);
    int rc = RC_OK;
    for (int i = 0; i < nr; i++) {
      const auto v1 = random_u32();
      const auto v2 = random_u32();
      const auto v3 = random_u32();
      const auto v4 = random_u32();
      q.cq.force(1, v1);
      q.cq.force(2, v2);
      q.cq.force(3, v3);
      q.cq.force(4, v4);
      const auto mo = random_u32();
      const auto m1 = random_u32();
      const auto m2 = random_u32();
      const auto m3 = random_u32();
      const auto m4 = random_u32();
      q.cq.mask(0, mo);
      q.cq.mask(1, m1);
      q.cq.mask(2, m2);
      q.cq.mask(3, m3);
      q.cq.mask(4, m4);
      const auto io = random_u32();
      const auto i1 = random_u32();
      const auto i2 = random_u32();
      const auto i3 = random_u32();
      const auto i4 = random_u32();
      q.cq.invert(0, io);
      q.cq.invert(1, i1);
      q.cq.invert(2, i2);
      q.cq.invert(3, i3);
      q.cq.invert(4, i4);
      const auto x1 = v1 ^ i1;
      const auto x2 = v2 ^ i2;
      const auto x3 = v3 ^ i3;
      const auto x4 = v4 ^ i4;
      const auto y1 = x1 & m1;
      const auto y2 = x2 & m2;
      const auto y3 = x3 & m3;
      const auto y4 = x4 & m4;
      q.cq.release_force(0);
      auto test = [&](value_t ref) {
        const auto v = q.out();
        const auto o = (ref^io)&mo;
        if (v != o) {
          std::cout << red << "ERROR: got 0x" << std::hex << v << " expected 0x" << std::hex << o << rst << std::endl;
          rc |= RC_ERROR_CHECK;
        }
        if (verb.veryverbose)
          std::cout << "qout=0x" << std::hex << v << std::endl;
      };
      q.cq.mode(comb_mode::SEL1);
      test(y1);
      q.cq.mode(comb_mode::SEL2);
      test(y2);
      q.cq.mode(comb_mode::SEL3);
      test(y3);
      q.cq.mode(comb_mode::SEL4);
      test(y4);
      q.cq.mode(comb_mode::AND);
      test(y1 & y2 & y3 & y4);
      q.cq.mode(comb_mode::OR);
      test(y1 | y2 | y3 | y4);
      q.cq.mode(comb_mode::XOR);
      test(y1 ^ y2 ^ y3 ^ y4);
      q.cq.mode(comb_mode::MAJ);
      test(bitwise_majority4(y1, y2, y3, y4));
      q.cq.mode(comb_mode::BLOCK8);
      test((y1 & 0xFF) + ((y2 & 0xFF) << 8) + ((y3 & 0xFF) << 16) + ((y4 & 0xFF) << 24));
      q.cq.mode(comb_mode::BLOCK16);
      test((y1 & 0xFFFF) + ((y2 & 0xFFFF) << 16));
      q.cq.mode(comb_mode::SUM12);
      test(y1+y2);
      q.cq.mode(comb_mode::SUM1234);
      test(y1+y2+y3+y4);
      q.cq.mode(comb_mode::DIFF12);
      test(y1-y2);
    }
    if (rc == RC_OK)
      std::cout << green << "SUCCESS." << rst << std::endl;
    return rc;
  }
  return RC_OK;
}

int ppaux(FPGA &fpga,
          const InputParser &input,
          [[maybe_unused]] const Verbosity &v)
{
  pio_in pio_aux(fpga.dev_lw, address_map::lw::pio_aux.base);
  const auto nr = parse_uint64(input, "-nr", "0");
  const auto wait = parse_double(input, "-wait", "0.5");
  const auto mode = input.get_string("-mode", "hex:bin:dec");
  const auto file = input.get_string("-file", "");
  const bool ctr = input.exists("-ctr");
  const bool ts = input.exists("-ts");
  std::ofstream F;
  if (file != "") {
    F.open(file);
    if (!F)
      throw std::runtime_error("Could not open AUX output file: " + file);
  }
  for (uint64_t cnt = 1; nr == 0 || cnt <= nr; cnt++) {
    const aux_t x = pio_aux.read();
    auto s = output_formatter(x, mode);
    if (ctr)
      s = std::to_string(cnt) + " " + s;
    if (ts)
      s = timestamp_iso8601_utc_ms() + " " + s;
    if (file != "") {
      F << s << '\n';
      if (!F)
        throw std::runtime_error("Failed writing AUX output file: " + file);
    } else {
      std::cout << s << std::endl;
    }
    sleepd(wait);
  }
  if (file != "") {
    F.close();
    if (!F)
      throw std::runtime_error("Failed closing AUX output file: " + file);
  }
  return RC_OK;
}
