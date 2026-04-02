// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Extensive library for tests of different functionalities

#pragma once

#include <cassert>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include "streamer.hh"
#include "readback.hh"
#include "parser.hh"
#include "PMODDA3.hh"
#include "sequence.hh"
#include "ppworkflow.hh"
#include "basic_multi_dma.hh"
#include "throttler.hh"
#include "timer.hh"

class tests {
public:
  FPGA &fpga;
  streamer &s;
  readback &rb;
  counter &ctr;
  trigger_int &trig_int;
  trigger_ext &trig_ext;
  const InputParser &input;
  streamer_fifo &fifo;  // s.fifo
  streamer_control &sc; // s.sc
  const Verbosity &verb;
  bool use_locks = true;

  // **** Basic tests for streaming out data
  int test0() {
    std::cout << "There is nothing to do!" << std::endl;
    return 0;
  }

  int test1() {
    std::cout << "test1 - empty sequence" << std::endl;
    Sequence elements;
    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb);
  }

  int test2() {
    std::cout << "test2 - one symbol" << std::endl;
    Sequence elements;
    auto c = parse_count(input, "-c", "1");
    auto v = parse_value(input, "-v", "0b11");
    elements.push_back(el(c, v));
    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb);
  }

  int test3() {
    std::cout << "test3 - counter" << std::endl;
    Sequence elements;
    auto c = parse_count(input, "-c", "1");
    auto v0 = parse_value(input, "-v0", "0");
    auto vmax = parse_value(input, "-v", "0b11");
    for (value_t v = v0; v < vmax; v++)
      elements.push_back(el(c, v));
    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb);
  }

  int test4() {
    std::cout << "test4 - sequential counter (or randomized values using the -rnd switch) with random duration" << std::endl;
    const auto c = parse_count(input, "-c", "10000");
    const auto vmax = parse_value(input, "-v", "10");
    const auto rnd = input.exists("-rnd");
    auto elements = prepare_random_test_sequence(vmax, c, rnd);
    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb);
  }

  int test5() {
    std::cout << "test5 - three symbols, one without strobes" << std::endl;
    Sequence elements;
    auto c = parse_count(input, "-c", "1");
    elements.push_back(el(c, 0b01));
    elements.push_back(el(NoStrobe(c), 0b10)); // use -timeout for testing
    elements.push_back(el(c, 0b11));
    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb, drop<1>);
  }

  int test6() {
    std::cout << "test6 - bitwise control" << std::endl;
    Sequence elements;
    auto c = parse_count(input, "-c", "1");
    elements.push_back(el(c, 0x00));
    elements.push_back(el(c, 0xff));
    elements.push_back(el(c, 0x00));

    elements.push_back(el(c, BitSet(0b0001)));
    elements.push_back(el(c, BitSet(0b0010)));
    elements.push_back(el(c, BitSet(0b0100)));
    elements.push_back(el(c, BitSet(0b1000)));

    elements.push_back(el(c, BitClear(0b0001)));
    elements.push_back(el(c, BitClear(0b0010)));
    elements.push_back(el(c, BitClear(0b0100)));
    elements.push_back(el(c, BitClear(0b1000)));

    elements.push_back(el(c,   BitSet(0b0001)));
    elements.push_back(el(c, BitClear(0b0001)));
    elements.push_back(el(c,   BitSet(0b0010)));
    elements.push_back(el(c, BitClear(0b0010)));
    elements.push_back(el(c,   BitSet(0b0100)));
    elements.push_back(el(c, BitClear(0b0100)));
    elements.push_back(el(c,   BitSet(0b1000)));
    elements.push_back(el(c, BitClear(0b1000)));

    elements.push_back(el(c, 0x00));

    elements.push_back(el(c, BitFlip(0b0001)));
    elements.push_back(el(c, BitFlip(0b0010)));
    elements.push_back(el(c, BitFlip(0b0100)));
    elements.push_back(el(c, BitFlip(0b1000)));

    elements.push_back(el(c, BitFlip(0b0001)));
    elements.push_back(el(c, BitFlip(0b0010)));
    elements.push_back(el(c, BitFlip(0b0100)));
    elements.push_back(el(c, BitFlip(0b1000)));

    elements.push_back(el(c, BitFlip(0b0001)));
    elements.push_back(el(c, BitFlip(0b0001)));
    elements.push_back(el(c, BitFlip(0b0010)));
    elements.push_back(el(c, BitFlip(0b0010)));
    elements.push_back(el(c, BitFlip(0b0100)));
    elements.push_back(el(c, BitFlip(0b0100)));
    elements.push_back(el(c, BitFlip(0b1000)));
    elements.push_back(el(c, BitFlip(0b1000)));

    elements.push_back(el(c, 0xff));
    elements.push_back(el(c, 0x00));
    elements.push_back(el(c, 0xff));
    elements.push_back(el(c, 0x00));
    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb, convert_for_readback_check);
  }

  int test7() {
    std::cout << "test7 - initial_value test" << std::endl;
    const auto initial_value = parse_value(input, "-iv", "4"); // cf. -i
    sc.set_initial_value(initial_value);
    sc.reset(); // reset of streamer core (not full FPGA fabric)
    auto qout = sc.get_qout();
    if (qout != initial_value) {
      std::cerr << "Mismatch: qout=" << std::hex << qout << " Expected: " << std::hex << initial_value << std::endl;
      return 1;
    }
    Sequence elements;
    if (input.exists("-ns")) elements.push_back(el(NoStrobe(1), initial_value));
    const auto c = parse_count(input, "-c", "1");
    const auto v = parse_value(input, "-v", "0b11");
    elements.push_back(el(c, v));
    send_and_trig(fifo, sc, rb, ctr, elements, input, do_not_force_trigger, verb); // don't trigger!
    qout = sc.get_qout();
    if (qout != initial_value) {
      std::cerr << "Mismatch: qout=" << std::hex << qout << " Expected: " << std::hex << initial_value << std::endl;
      return 1;
    } else {
      std::cout << green << "SUCCESS" << rst << std::endl;
    }
    return 0;
  }

  int test8() {
    std::cout << "test8 - randomized testing of updates" << std::endl;
    const auto c = parse_count(input, "-c", "10000");
    const auto vmax = parse_value(input, "-v", "10");
    auto elements = prepare_general_random_test_sequence(vmax, c);
    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb, convert_for_readback_check);
  }

  int test9() {
    std::cout << "test9 - test qout_override" << std::endl;
    Sequence elements;
    auto c = parse_count(input, "-c", "1");
    auto v = parse_value(input, "-v", "0b11");
    elements.push_back(el(c, v));
    int rc = send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb);
    auto q = parse_value(input, "-q", "0x12345678");
    sc.set_qout_override(q);
    sc.qout_select(output_override);
    const auto qout = sc.get_qout();
    const auto qout_streamer = sc.get_qout_streamer();
    std::cout << "q_override=0x" << std::hex << q << " qout=0x" << qout << " qout_streamer=0x" << qout_streamer << std::endl;
    if (q == qout) {
      std::cout << green << "SUCCESS" << rst << std::endl;
      return 0 | rc;
    } else {
      std::cout << red << "FAILURE" << rst << std::endl;
      return 0x2 | rc;
    }
  }

  // **** Advanced functionality tests
  int test10() {
    std::cout << "test10 - preprocessor" << std::endl;
    Sequence elements;

    auto p = parse_count(input, "-p", "1"); // length of -pre 0xAA run
    if (input.exists("-pre"))
      elements.push_back(el(p, 0xAA)); // -pre: regular element at the very beginning of the sequence

    auto c = parse_count(input, "-c", "1");
    auto vmax = parse_value(input, "-v", "3");
    assert(vmax >= 0 && vmax <= POSITIONS); // nr. positions in the fast memory (0 is allowed)
    for (value_t v = 0; v < vmax; v++)
      elements.push_back(el(c, v).store(v)); // store at position v, ranging from 0 to vmax-1

    auto m = parse_count(input, "-m", "1"); // length of -mid 0xBB run
    if (input.exists("-mid"))
      elements.push_back(el(m, 0xBB)); // -mid: regular element between store and replay elements

    auto repetitions = parse_count(input, "-repetitions", "10");
    auto nr_replays = parse_count(input, "-nr_replays", "1");
    auto s =  parse_count(input, "-s", "1"); // length of -sep 0xFF run
    for (size_t n = 0; n < nr_replays; n++) {
      elements.push_back(el(Replay{}, repetitions, vmax)); // replay positions [0:vmax-1] 'repetitions' times
      if (n != nr_replays-1 && input.exists("-sep"))
        elements.push_back(el(s, 0xFF)); // -sep: regular element between each invocation of replay
    }

    auto o = parse_count(input, "-o", "1"); // length of -post 0xCC run
    if (input.exists("-post")) // -post: regular element at the very end of the sequence
    elements.push_back(el(o, 0xCC));

    // ** Generate an equivalent sequence
    Sequence el_ref;
    if (input.exists("-pre"))
      el_ref.push_back(el(p, 0xAA));
    if (input.exists("-mid"))
      el_ref.push_back(el(m, 0xBB));
    for (size_t n = 0; n < nr_replays; n++) {
      for (size_t i = 0; i < repetitions; i++)
        for (value_t v = 0; v < vmax; v++)
          el_ref.push_back(el(c, v));
      if (n != nr_replays-1 && input.exists("-sep"))
        el_ref.push_back(el(s, 0xFF));
    }
    if (input.exists("-post"))
      el_ref.push_back(el(o, 0xCC));
    el_ref.push_back(el()); // final
    el_ref = el_ref.merge();

    auto replace_with_ref = [el_ref](Sequence &e){ e = el_ref; };
    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb, replace_with_ref);
  }

  int test11() {
    std::cout << "test11 - infinite replay" << std::endl;
    Sequence elements;
    auto c = parse_count(input, "-c", "10");
    auto v1 = parse_value(input, "-v1", "0xFFFFFFFF");
    auto v0 = parse_value(input, "-v0", "0x00000000");
    elements.push_back(el(c, v0).store(0));
    elements.push_back(el(c, v1).store(1));
    elements.push_back(el(Replay{}, 0, 2)); // 0 = repeat indefinitely
    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb);
  }

  // **** Triggering tests
  static void trig12(trigger_int &trig_int, const InputParser &input) {
    if (input.exists("-trig")) {
      constexpr int delay = 100*1000;
      usleep(delay);
      trig_int.write(1);
      std::cout << "## Trigger sequence emitted." << std::endl;
      usleep(delay);
      trig_int.write(0);
    }
  }

  int test12() {
    std::cout << "test12 - simple trigger, one symbol" << std::endl;
    Sequence elements;
    auto p = parse_trigger(input, "-p", "0b1");
    auto m = parse_trigger(input, "-m", "0b1");
    elements.push_back(el(p, m, true));
    auto c = parse_count(input, "-c", "1");
    auto v = parse_value(input, "-v", "0b11");
    elements.push_back(el(c, v));
    std::thread trig(trig12, std::ref(trig_int), std::ref(input));
    int rc = send_and_trig(fifo, sc, rb, ctr, elements, input, do_not_force_trigger, verb);
    trig.join();
    return rc;
  }

  static void trig13(trigger_int &trig_int, const InputParser &input) {
    if (input.exists("-trig")) {
      auto p = parse_trigger(input, "-p", "0b01");
      auto r = parse_trigger(input, "-r", "0b10");
      constexpr int delay = 200*1000;
      usleep(delay);
      trig_int.write(p);
      usleep(delay);
      trig_int.write(r);
      std::cout << "## Trigger sequence emitted." << std::endl;
      usleep(delay);
      trig_int.write(0);
    }
  }

  int test13() {
    std::cout << "test13 - two-stage trigger, one symbol" << std::endl;
    Sequence elements;
    auto p = parse_trigger(input, "-p", "0b01");
    auto m = parse_trigger(input, "-m", "0b01");
    elements.push_back(el(p, m, false));
    auto r = parse_trigger(input, "-r", "0b10");
    auto n = parse_trigger(input, "-n", "0b10");
    elements.push_back(el(r, n, true));
    auto c = parse_count(input, "-c", "1");
    auto v = parse_value(input, "-v", "0b11");
    elements.push_back(el(c, v));
    std::thread trig(trig13, std::ref(trig_int), std::ref(input));
    int rc = send_and_trig(fifo, sc, rb, ctr, elements, input, do_not_force_trigger, verb);
    trig.join();
    return rc;
  }

  static void trig14(trigger_int &trig_int, const InputParser &input) {
    if (input.exists("-trig")) {
      const auto cycles = parse_uint32(input, "-cycles", "10");
      const auto p = parse_trigger(input, "-p", "0b01");
      const auto r = parse_trigger(input, "-r", "0b10");
      const auto delay = parse_uint32(input, "-delay", "10000");
      for (uint32_t i = 0; i < cycles; i++) {
        usleep(delay);
        trig_int.write(p);
        usleep(delay);
        trig_int.write(r);
      }
      std::cout << "## Trigger sequence emitted." << std::endl;
      usleep(delay);
      trig_int.write(0);
    }
  }

  int test14() {
    std::cout << "test14 - multi-stage trigger, one symbol" << std::endl;
    Sequence elements;
    const auto cycles = parse_uint32(input, "-cycles", "10");
    const auto p = parse_trigger(input, "-p", "0b01");
    const auto m = parse_trigger(input, "-m", "0b01");
    const auto r = parse_trigger(input, "-r", "0b10");
    const auto n = parse_trigger(input, "-n", "0b10");
    for (size_t i = 0; i < cycles; i++) {
      elements.push_back(el(p, m, false));
      elements.push_back(el(r, n, (i == cycles-1)));
    }
    const auto c = parse_count(input, "-c", "1");
    const auto v = parse_value(input, "-v", "0b11");
    elements.push_back(el(c, v));
    std::thread trig(trig14, std::ref(trig_int), std::ref(input));
    const int rc = send_and_trig(fifo, sc, rb, ctr, elements, input, do_not_force_trigger, verb);
    trig.join();
    return rc;
  }

  static void trig15(trigger_int &trig_int, const InputParser &input) {
    if (input.exists("-trig")) {
      const auto cycles = parse_uint32(input, "-cycles", "10");
      const auto p = parse_trigger(input, "-p", "0b01");
      const auto r = parse_trigger(input, "-r", "0b10");
      const auto delay = parse_uint32(input, "-delay", "100000"); // 100ms
      for (uint32_t i = 0; i < cycles; i++) {
        usleep(delay);
        trig_int.write(i % 2 == 0 ? p : r);
      }
      std::cout << "## Trigger sequence emitted." << std::endl;
      usleep(delay);
      trig_int.write(0);
    }
  }

  int test15() {
    std::cout << "test15 - retriggering" << std::endl;
    Sequence elements;
    const auto cycles = parse_uint32(input, "-cycles", "10");
    const auto c = parse_count(input, "-c", "1000000"); // appropriate number depends on streamer_clk frequency
    const auto v = parse_count(input, "-v", "1");
    const auto p = parse_trigger(input, "-p", "0b01");
    const auto m = parse_trigger(input, "-m", "0b01");
    const auto r = parse_trigger(input, "-r", "0b10");
    const auto n = parse_trigger(input, "-n", "0b10");
    for (size_t i = 0; i < cycles; i++) {
      if (i % 2 == 0)
        elements.push_back(el(p, m, true));
      else
        elements.push_back(el(r, n, true));
      for (size_t j = 0; j < v; j++) {
        const value_t val = i + (j << 16); // Lowest 16 bits: cycle number, highest 16 bits: run (block) number
        elements.push_back(el(c, val));
      }
      if (i < cycles-1)
        elements.push_back(el(Retrig{}));
    }
    std::thread trig(trig15, std::ref(trig_int), std::ref(input));
    const int rc = send_and_trig(fifo, sc, rb, ctr, elements, input, do_not_force_trigger, verb);
    trig.join();
    return rc;
  }

  int test16() {
    std::cout << "test16 - periodic signals with periods linear in pin index" << std::endl;
    const auto cycles = parse_uint32(input, "-cycles", "1024");
    const auto c = parse_count(input, "-c", "5000000");
    if (verb.verbose) {
      std::cout << "cycles=" << cycles << std::endl;
      std::cout << "c=" << c << std::endl;
    }
    for (size_t i = 0; cycles == 0 || i < cycles; i++) {
      value_t v = 0;
      for (int j = 0; j < WIDTH_DATA; j++)
        if (i % (j+1) == 0) // flip periodically! period: 1,2,3,4,...
          v += 1UL << j;
      const el e {c, BitFlip(v)};
      fifo.out(e);
      if (i == max_size-1)
        sc.trigger_force();
      if (verb.veryverbose && i > max_size && i % 100 == 0) {
        fifo.report();
        s.sc.status_report();
      }
    }
    if (verb.verbose)
      s.sc.status_report();
    return 0;
  }

  // ****  Complex tests
  int test19() {
    std::cout << "test19 - pseudorandom number test (xoroshift128+)" << std::endl;
    Sequence elements;
    auto c = parse_count(input, "-c", "1");
    if (c > 0) {
      elements.push_back(el(PseudoRandom{}, c));
    } else {
      elements.push_back(el(PseudoRandom{}, 0xffffffff).store(0));
      elements.push_back(el(Replay{}, 0, 1)); // 0 = repeat indefinitely
    }
    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb);
  }

  static const size_t polling_sleep_time = 10;

  // Writes to the streamer, and adds the same elements to Sequence 'elements' for verification in a
  // separate thread. See function reader() below.
  static void writer(FPGA &fpga, streamer_fifo &fifo, streamer_control &sc, const InputParser &input,
                      const Verbosity &v, Sequence &elements, const bool use_locks) {
    const auto nr = parse_value(input, "-v", "0"); // 0 = infinity
    value_t value = 0;
    const bool rnd = input.exists("-rnd");
    value_t previous_value = 0; // for randomized tests to prevent collisions
    const bool log_len = input.exists("-log_len");
    const auto min_len = parse_count(input, "-cmin", "1");
    const auto max_len = parse_count(input, "-c", "1000000");
    for (unsigned long i = 0; nr == 0 || i < nr; i++) {
      if (use_locks) fpga.lock();
      if (rnd) {
        do {
          value = random_u32();
        } while (value == previous_value);
      } else {
        value = i;
      }
      // blocks of random length
      count_t len = (log_len ? random_log_uniform(min_len, max_len) : random_lin_uniform(min_len, max_len));
      el e{len, value};
      fifo.out(e, v.verbose);
      elements.push_back(e);
      if (i == 10)
        sc.trigger_force();
      if (use_locks) fpga.unlock();
      usleep(polling_sleep_time);
      previous_value = value;
    }
    auto e = el();
    if (use_locks) fpga.lock();
    fifo.out(e, v.verbose);
    if (use_locks) fpga.unlock();
    //  elements.push_back(e);
  }

  static int reader(FPGA &fpga, readback &rb, const InputParser &input,
                    const Verbosity &v, Sequence &elements, const bool use_locks) {
    const int reporting_period = parse_uint32(input, "-report", "1"); // seconds
    Throttler thr(reporting_period);
    Timer t;
    size_t n = 0;
    uint64_t len = 0; // can get big!
    size_t n_error = 0;
    while (1) {
      if (use_locks) fpga.lock();
      auto fill = rb.filled();
      if (fill > 0) {
        auto e = rb.read();
        if (v.verbose) std::cout << "C " << e << std::endl;
        auto e_ref = elements.front();
        elements.pop_front();
        if (e != e_ref) {
          std::cerr << "ERROR - RECEIVED: " << e << " EXPECTED: " << e_ref << std::endl;
          n_error++;
        }
        n++;
        len += e.count();
        thr.try_call([n,len,&t]{
          std::cout << "run time=" << std::fixed << std::setprecision(3) << double(t.elapsed<std::chrono::milliseconds>().count())/1000 << "s"
            << " size=" << std::dec << n << " length=" << with_underscores(len) << std::endl;
        });
        fill = rb.filled();
      }
      if (use_locks) fpga.unlock();
      usleep(polling_sleep_time);
      if (n && elements.empty()) {
        std::cout << "Checker report: " << n_error << " errors for " << n << " elements checked, error ratio=" << double(n_error)/n << std::endl;
        std::cout << (n_error ? red : green) << (n_error ? "#### FAILURE ####" : "SUCCESS") << rst << std::endl;
        break;
      }
    }
    return n_error > 0;
  }

  int test20() {
    std::cout << "test20 - continuous mode" << std::endl;
    Sequence elements;
    if (input.exists("-nolocks")) {
      use_locks = false;
      if (verb.verbose) std::cout << "Locking disabled." << std::endl;
    }
    std::thread wr(writer, std::ref(fpga), std::ref(fifo), std::ref(sc), std::ref(input), std::ref(verb), std::ref(elements), use_locks);
    auto rc = std::async(std::launch::async, reader, std::ref(fpga), std::ref(rb), std::ref(input), std::ref(verb), std::ref(elements), use_locks);
    wr.join();
    return rc.get();
  }

  int test21() {
    std::cout << "test21 - PMOD DA3 SPI voltage sweep" << std::endl;

    const auto vmin = parse_double(input, "-vmin", "0.0");
    const auto vmax = parse_double(input, "-vmax", "2.5");
    const auto vstep = parse_double(input, "-vstep", "0.01");
    const auto dwell = parse_time(input, "-dwell", "10ms");
    auto cfg = pmod_da3::default_spi_config();
    cfg.spi_clock_hz = parse_double(input, "-spi_clock", "10e6");

    double streamer_clk_hz = 100e6;
    try {
      streamer_clk_hz = fpga.streamer_freq();
    } catch (const std::exception &) {
      if (verb.verbose)
        std::cout << "streamer clock not measured, assuming " << streamer_clk_hz << " Hz" << std::endl;
    }
    cfg.decoder_clock_hz = streamer_clk_hz;

    if (!(vstep > 0.0))
      throw std::runtime_error("test21 requires a positive -vstep");
    if (!(dwell > 0.0))
      throw std::runtime_error("test21 requires a positive -dwell");
    if (vmax < vmin)
      throw std::runtime_error("test21 requires the upper voltage bound to be at least the lower bound");

    const auto dwell_ticks = static_cast<count_t>(std::llround(dwell * streamer_clk_hz));
    if (dwell_ticks == 0)
      throw std::runtime_error("test21 dwell time is shorter than one streamer clock period");

    if (verb.verbose) {
      std::cout << "vmin=" << vmin << " vmax=" << vmax << " vstep=" << vstep << " dwell=" << dwell << "s" << std::endl;
      std::cout << "requested SPI clock=" << cfg.spi_clock_hz << "Hz achieved SPI clock="
                << pmod_da3::transaction_for_voltage(vmin, 2.5, cfg).achieved_spi_clock_hz() << "Hz" << std::endl;
    }

    Sequence elements;
    size_t points = 0;
    for (double volts = vmin; volts <= vmax + 0.5 * vstep; volts += vstep) {
      auto builder = pmod_da3::transaction_for_voltage(volts, 2.5, cfg);
      const auto &tx = builder.sequence();
      elements.insert(elements.end(), tx.begin(), tx.end());
      elements.push_back(el(dwell_ticks, tx.back().value()));
      points++;
    }
    elements = elements.merge();

    if (verb.verbose)
      std::cout << "Generated " << std::dec << points << " DAC updates, sequence size=" << elements.size() << std::endl;

    return send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger, verb);
  }

  int test42() {
    std::cout << "test42 - stream a sequence from a file" << std::endl;
    const std::string fn = input.get_string("-f", "sequence");
    if (verb.verbose) std::cout << "Reading " << fn << std::endl;
    int rc;
    try{
      std::ifstream f(fn);
      auto [elements, force_trigger_p] = parse_sequence_from_stream(f);
      rc = send_and_trig(fifo, sc, rb, ctr, elements, input, force_trigger_p, verb);
    } catch (const std::exception& e) {
      std::cout << "Caught exception: " << e.what() << std::endl;
      rc = 1;
    }
    return rc;
  }

  tests(FPGA &_fpga, streamer &_s, readback &_rb, counter &_ctr, const InputParser &_input, const Verbosity &_v) :
    fpga(_fpga), s(_s), rb(_rb), ctr(_ctr), trig_int(fpga.trig_int), trig_ext(fpga.trig_ext), input(_input), fifo(s.fifo), sc(s.sc), verb(_v) {}

  int run(int test) {
    std::cout << "Requested test " << std::dec << test << std::endl;
    int rc = 0;
    switch (test) {
    case 0:
      rc = test0();
      break;
    case 1:
      rc = test1();
      break;
    case 2:
      rc = test2();
      break;
    case 3:
      rc = test3();
      break;
    case 4:
      rc = test4();
      break;
    case 5:
      rc = test5();
      break;
    case 6:
      rc = test6();
      break;
    case 7:
      rc = test7();
      break;
    case 8:
      rc = test8();
      break;
    case 9:
      rc = test9();
      break;
    case 10:
      rc = test10();
      break;
    case 11:
      rc = test11();
      break;
    case 12:
      rc = test12();
      break;
    case 13:
      rc = test13();
      break;
    case 14:
      rc = test14();
      break;
    case 15:
      rc = test15();
      break;
    case 16:
      rc = test16();
      break;
    case 19:
      rc = test19();
      break;
    case 20:
      rc = test20();
      break;
    case 21:
      rc = test21();
      break;
    case 42:
      rc = test42();
      break;
    default:
      std::cerr << "Unknown test " << test << std::endl;
      break;
    }
    return rc;
  }
};
