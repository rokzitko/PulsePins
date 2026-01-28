// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// pptool main() is here...

#include "ppcommon.hh"
#include "ppmisc.hh"
#include "pptest.hh"

// First command line argument is the test number (pptest, ppmstest, ppdmatest)
auto get_test_number(int argc, char *argv[])
{
  int test = 1;
  if (argc >= 2 && isdigit(argv[1][0]))
    test = atoi(argv[1]);
  return test;
}

int pptest(InputParser &input, int argc, char *argv[], Verbosity &v)
{
  const int test = get_test_number(argc, argv);
  int rc = 0; // return code
  try {
    FPGA fpga(v);
    streamer s(input, fpga);
    trigger tr(input, fpga);
    readback rb(input, fpga);
    counter ctr(input, fpga);
    ctr.reset_all();
    pio_out pio_trig_int(fpga.dev_lw, PIO_TRIG_INT_BASE); // Used for trigger circuit testing
    pio_trig_int.write(0); // no trigger signals present initially
    trigger_ext trig_ext(fpga.dev_lw, PIO_TRIG_MONITOR_BASE);
    tests t(s, rb, ctr, pio_trig_int, trig_ext, input, v);
    rc = t.run(test);
  }
  catch (const char *e) {
    std::cout << "exception: " << e << std::endl;
  }
  if (input.exists("-ignore-errors") && (rc != 0)) {
    std::cout << "WARNING: Ignoring errors, return code reset to zero." << std::endl;
    rc = 0;
  }
  std::cout << "All done, exiting with return code " << std::dec << rc << std::endl;
  return rc;
}

#include "ppmstest.hh"

int ppmstest(InputParser &input, int argc, char *argv[], Verbosity &v)
{
  const int test = get_test_number(argc, argv);
  int rc = 0; // return code
  try {
    FPGA fpga(v);
    multistreamer s(input, fpga);
    qout q(input, v, fpga);
    readback rb(input, fpga);
    pio_out pio_trig_int(fpga.dev_lw, PIO_TRIG_INT_BASE);
    pio_trig_int.write(0); // no trigger signals present initially
    mstests t(s, q, rb, pio_trig_int, input, v);
    rc = t.run(test);
  }
  catch (const char *e) {
    std::cout << "exception: " << e << std::endl;
  }
  if (input.exists("-ignore-errors") && (rc != 0)) {
    std::cout << "WARNING: Ignoring errors, return code reset to zero." << std::endl;
    rc = 0;
  }
  std::cout << "All done, exiting with return code " << std::dec << rc << std::endl;
  return rc;
}

#include "ppdmatest.hh"

int ppdmatest(InputParser &input, int argc, char *argv[], Verbosity &v)
{
  const int test = get_test_number(argc, argv);
  int rc = 0; // return code
  try {
    FPGA fpga(v);
    dma_streamer s(input, fpga);
    readback rb(input, fpga);
    counter ctr(input, fpga);
    dmatests t(s, rb, ctr, input, v);
    rc = t.run(test);
  }
  catch (const char *e) {
    std::cout << "exception: " << e << std::endl;
  }
  if (input.exists("-ignore-errors") && (rc != 0)) {
    std::cout << "WARNING: Ignoring errors, return code reset to zero." << std::endl;
    rc = 0;
  }
  std::cout << "All done, exiting with return code " << std::dec << rc << std::endl;
  return rc;
}

#include "ppfg.hh"

// Parse trigger pattern and mask
std::pair<trigger_t, trigger_t> get_trigger_pm(const InputParser &input, const bool verbose)
{
  const auto p = parse_trigger(input, "-p", "0b00000001");
  const auto m = parse_trigger(input, "-m", "0b00000001");
  if (verbose)
    std::cout << "Trigger: pattern=" << std::bitset<WIDTH_TRIGGER>(p)
      << " mask=" << std::bitset<WIDTH_TRIGGER>(m) << std::endl;
  return {p, m};
}

// Frequency generator
int ppfg(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  FPGA fpga(v);
  streamer s(input, fpga); // must be called first to setup the PLL

  // Determine signal lengths based on period/frequency and duty cycle settings
  auto period_req = parse_period(input);                 // period (requested) [s]
   auto duty = input.get_double("-duty", 50.0);          // duty cycle [percentage]
  if (input.exists("-servo")) {                          // for testing servo motors...
    const double angle = input.get_double("-servo", 90); // rotation angle
    const auto [f, d] = servo_pwm_params(angle);
    period_req = 1.0/f;
    duty = d;
  }
  const double output_clk = s.int_clk.get_freq(0);
  if (v.verbose)
    std::cout << "output_clk=" << pretty_frequency(output_clk) << " output_clk_period="
    << pretty_time(1.0/output_clk) << std::endl;
  const auto [nr_pos, nr_neg] = calc_pos_neg(period_req, duty, output_clk);

  // The two output patterns. If start0=true, we start with v0, otherwise we start with v1.
  const auto v1 = parse_value(input, "-v1", "0xFFFFFFFF");
  const auto v0 = parse_value(input, "-v0", "0x00000000");
  const bool start0 = input.exists("-start0");

  // Delay after trigger
  const auto delay = parse_time(input, "-delay", "0"); // delay after trigger (requested)
  const auto nr_delay = calc_delay(delay, output_clk);

  // Trigger settings.
  trigger tr(input, fpga);
  const auto [p, m] = get_trigger_pm(input, v.verbose);
  const bool autotrig = input.exists("-trig") || input.exists("-autotrig");

  // Gate settings
  s.sc.set_gating_from_string(input.get_string("-gate", ""));
  if (input.exists("-gate_debug")) {
    for (;;) {
      usleep(100*1000);
      std::cout << "Gate: " << s.sc.gate_status_string() << std::endl;
    }
  }

  // *** Continuous mode
  if (input.exists("-cont")) {
    if (v.verbose)
      std::cout << "Continuous mode" << std::endl;
    const auto seq = seq_continuous(p, m, nr_delay, nr_pos, nr_neg, v1, v0, start0);
    if (v.veryverbose)
      seq.dump(std::cout, "| ");
    s.fifo.send_sequence(seq);
    if (autotrig)
      s.sc.trigger_force();
    else
      s.sc.trigger_enable();
    for (;;) {}
  }

  // *** Burst mode
  if (input.exists("-burst")) {
    const auto rep = input.get_uint32("-burst", 1);
    if (v.verbose)
      std::cout << "Burst mode: repetitions=" << rep << std::endl;
    const auto final = parse_value(input, "-t", "0");
    const auto seq = seq_burst(p, m, nr_delay, nr_pos, nr_neg, v1, v0, rep, start0, final);
    if (v.veryverbose)
      seq.dump(std::cout, "| ");
    const auto seq_retrig = with_pushed(seq, el(Retrig{}));
    const auto seq_final  = with_pushed(seq, el(final));  // this is a terminating element

    const count_t n_max = parse_uint32(input, "-n_max", "1");
    for (count_t n = 0; n_max == 0 || n < n_max; n++) {
      if (n_max == 0 || n < n_max-1)
        s.fifo.send_sequence(seq_retrig);
      else
        s.fifo.send_sequence(seq_final);
      if (n == 0) {
        if (autotrig)
          s.sc.trigger_force();
        else
          s.sc.trigger_enable();
      }
    }
  }
  std::cout << "All done, exiting." << std::endl;
  return 0;
}

count_t calc_duration_nr(const double duration_req,
                         const bool verbose,
                         const double output_clk = default_output_clk)
{
  const double output_clk_period = 1.0/output_clk;
  if (verbose)
    std::cout << "output_clk=" << pretty_frequency(output_clk) << " output_clk_period="
    << pretty_time(output_clk_period) << std::endl;
  uint64_t nr = round(duration_req/output_clk_period);
  nr = (nr > 0 ? nr : 1); // minimal sensible value here is 1
  const double duration_resulting = nr*output_clk_period;
  std::cout << "duration (requested)=" << pretty_time(duration_req) << " (resulting)="
    << pretty_time(duration_resulting) << " nr=" << std::dec << nr << std::endl;
  if (nr > max_count_t)
    throw std::runtime_error("Duration exceeds the limit of the count_t timer range.");
  return nr;
}

int ppdelay(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  FPGA fpga(v);
  streamer s(input, fpga);
  trigger tr(input, fpga);
  const auto [p, m] = get_trigger_pm(input, v.verbose);
  const auto delay = parse_time(input, "-delay", "0");
  const double output_clk = s.int_clk.get_freq(0);
  const auto nr_delay = calc_delay(delay, output_clk);
  const auto pulse_duration = parse_time(input, "-duration", "0");
  const auto nr = calc_duration_nr(pulse_duration, v.verbose, output_clk);
  const auto nr_neg = 1;
  const auto v1 = parse_value(input, "-v1", "0xFFFFFFFF");
  const auto v0 = parse_value(input, "-v0", "0x00000000");
  const auto final = parse_value(input, "-t", "0");
  auto seq = seq_once(p, m, nr_delay, nr, nr_neg, v1, v0, final);
  if (v.veryverbose) seq.dump(std::cout, "| ");
  s.fifo.send_sequence(seq);
  s.sc.trigger_enable();
  return 0;
}

int ppreset(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  FPGA fpga(v);
  streamer s(input, fpga); // reset is performed in streamer constructor
  std::cout << "All done, exiting." << std::endl;
  return 0;
}

int pptrig(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  FPGA fpga(v);
  streamer s(input, fpga);
  trigger tr(input, fpga);
  pio_out pio_trig_int(fpga.dev_lw, PIO_TRIG_INT_BASE); // for testing internal trigger path
  auto p = parse_uint32(input, "-pio", "0");
  pio_trig_int.write(p);
  if (v.veryverbose) {
    auto report = [](std::string s, value_t v) {
      std::cout << "tr(" << s << ")=0x" << std::hex << std::setfill('0') << std::setw(2) << (v && 0xFF) << " " << std::bitset<WIDTH_TRIGGER>(v) << std::endl;
    };
    report("int",  tr.ct.in1());
    report("ext",  tr.ct.in2());
    report("misc", tr.ct.in3());
    report("out",  tr.ct.out());
  }
  if (input.exists("-debug"))
    s.sc.monitor_ext_trig();
  std::cout << "All done, exiting." << std::endl;
  return 0;
}

int ppqout(const InputParser &input, int argc, char *argv[], const Verbosity &verb)
{
  FPGA fpga(verb);
  multistreamer s(input, fpga);
  qout q(input, verb, fpga);
  if (input.exists("-self_test")) {
    auto rc = q.cq.self_test();
    if (rc != 0) {
      std::cout << red << "Self test failed: rc=" << rc << rst << std::endl;
      return rc;
    }
  }
  if (input.exists("-report_pre"))
    q.cq.report();
  s.s1.sc.qout_set(parse_value(input, "-q1", "0"));
  s.s2.sc.qout_set(parse_value(input, "-q2", "0"));
  s.s3.sc.qout_set(parse_value(input, "-q3", "0"));
  s.s4.sc.qout_set(parse_value(input, "-q4", "0"));
  usleep(1);
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
    int rc = 0;
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
          rc = 1;
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
    if (rc == 0)
      std::cout << green << "SUCCESS." << rst << std::endl;
    return rc;
  }
  return 0;
}

int ppaux(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  FPGA fpga(v);
  pio_in pio_aux(fpga.dev_lw, PIO_AUX_BASE);
  const auto nr = parse_uint64(input, "-nr", "0");            // 0 = infinity
  const auto wait = parse_double(input, "-wait", "0.5");      // in seconds
  const auto mode = input.get_string("-mode", "hex:bin:dec"); // output formatting mode
  const auto file = input.get_string("-file", "");            // output to file if non-empty (filename)
  const bool ctr = input.exists("-ctr");                      // include counter (1 based)
  const bool ts = input.exists("-ts");                        // include timestamp
  std::ofstream F;
  if (file != "")
    F.open(file);
  for (uint64_t cnt = 1; nr == 0 || cnt <= nr; cnt++) {
    const aux_t x = pio_aux.read();
    auto s = output_formatter(x, mode);
    if (ctr)
      s = std::to_string(cnt) + " " + s;
    if (ts)
      s = timestamp_iso8601_utc_ms() + " " + s;
    if (file != "")
      F << s << '\n';
    else
      std::cout << s << std::endl;
    usleep(int(wait * 1000 * 1000));
  }
  std::cout << "All done, exiting." << std::endl;
  return 0;
}

#include "counter.hh"

int ppcounter(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  int rc = 0;
  FPGA fpga(v);
  streamer s(input, fpga);
  counter ctr(input, fpga);
  ctr.reset_all();
  Sequence seq;
  if (input.exists("-test1"))
    seq = counter_seq1();
  if (input.exists("-test2"))
    seq = counter_seq2(input);
  if (v.veryverbose)
    seq.dump(std::cout, "| ");
  s.fifo.send_sequence(seq);
  s.sc.trigger_force();
  s.sc.wait_to_complete(v);
  ctr.latch_all();
  ctr.report();
  if (input.exists("-test1") && input.exists("-check"))
    rc = counter_test1(ctr);
  std::cout << "All done, exiting." << std::endl;
  return rc;
}

int ppread(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  FPGA fpga(v, false);       // false=do not automatically assert the output enable signal
  if (input.exists("-oe")) { // if -oe not specified, leave as is!
    const bool oe = parse_bool(input, "-oe", "0");
    fpga.output_enable(oe);
  }
  readback rb(input, fpga);
  if (v.veryverbose)
    rb.check_fill_status();
  const double timeout = readback_timeout(input);
  rb.read_all(timeout);
  std::cout << "All done, exiting." << std::endl;
  return 0;
}

#include "timestamp.hh"

std::mutex lockcout;

void ts_reader(const InputParser &input, std::string label, std::function<uint64_t()> read)
{
  try {
    uint64_t previous = 0;
    const auto nr = parse_uint64(input, "-nr", "0");      // 0 = infinity
    for (uint64_t ctr = 0; nr == 0 || ctr <= nr; ctr++) { // null + nr more
      const auto current = read();
      lockcout.lock();
      std::cout << label << " ctr=" << with_underscores(ctr) << " ts=" << with_underscores(current);
      if (ctr) {
        const auto diff = current-previous;
        std::cout << " diff=" << with_underscores(diff);
      }
      std::cout << std::endl;
      lockcout.unlock();
      previous = current;
    }
  } catch (const std::runtime_error &e) {
    std::cout << "Exception: " << e.what() << std::endl;
  }
}

int ppts(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  FPGA fpga(v);
  rstmgr rm;
  rm.s2f_reset(); // FPGA fabric reset
  timestamp ts(fpga.dev_h2f,
               fpga.dev_lw,
               FIFO_TS_PPS_OUT_BASE, FIFO_TS_PPS_IN_CSR_BASE,
               FIFO_TS_SIGA_OUT_BASE, FIFO_TS_SIGA_IN_CSR_BASE,
               PIO_CFG_BASE);
  const double timeout = parse_double(input, "-timeout", "0"); // timeout for reading new events from FIFO timestamp buffer
  const bool read_pps = true; // always read pulse-per-second reference
  if (input.exists("-pps_in"))
    ts.sel_pps_in();
  if (input.exists("-pps_xtal"))
    ts.sel_pps_xtal();
  std::thread ts_pps;
  if (read_pps)
    ts_pps = std::thread(ts_reader, std::ref(input), "PPS", [&ts, timeout](){ return ts.read_with_timeout(timeout); });
  const bool read_sigA = input.exists("-sigA"); // enable reading signal A
  const auto selA = parse_uint32(input, "-selA", "0"); // select the source for signal A
  ts.selA(selA);
  if (v.verbose)
    std::cout << "timestamp configuration=" << ts.get_cfg() << std::endl;
  std::thread ts_sigA;
  if (read_sigA)
    ts_sigA = std::thread(ts_reader, std::ref(input), "sigA", [&ts, timeout](){ return ts.readA_with_timeout(timeout); });
  if (read_pps) ts_pps.join();
  if (read_sigA) ts_sigA.join();
  std::cout << "All done, exiting." << std::endl;
  return 0;
}

#include "zip_aggregator.hh"
#include "PID.hh"

// signum function (-1, 0, 1), returns int
template <typename T> int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

int ppgpsdo(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  FPGA fpga(v);
  rstmgr rm;
  rm.s2f_reset(); // FPGA fabric reset
  timestamp ts(fpga.dev_h2f,
               fpga.dev_lw,
               FIFO_TS_PPS_OUT_BASE, FIFO_TS_PPS_IN_CSR_BASE,
               FIFO_TS_SIGA_OUT_BASE, FIFO_TS_SIGA_IN_CSR_BASE,
               PIO_CFG_BASE);
  const double timeout = parse_double(input, "-timeout", "0"); // timeout for reading new events from FIFO timestamp buffer
  if (input.exists("-pps_in"))
    ts.sel_pps_in();
  if (input.exists("-pps_xtal"))
    ts.sel_pps_xtal();
  const auto selA = parse_uint32(input, "-selA", "0"); // select the source for signal A
  ts.selA(selA);
  if (v.verbose)
    std::cout << "timestamp configuration=" << ts.get_cfg() << std::endl;
  const auto kp = parse_double(input, "-kp", "0.01"); // proportional
  const auto ki = parse_double(input, "-ki", "0.1"); // integral
  int64_t clip = parse_uint64(input, "-clip", "1000"); // clip large error values
  int64_t reject = parse_uint64(input, "-reject", "10000"); // reject large error values (e.g. spurious edges detected)
  const auto dp = parse_uint32(input, "-dp", "0"); // deadband for P
  const auto di = parse_uint32(input, "-di", "0"); // deadband for I
  const auto eps = parse_double(input, "-eps", "0.0"); // epsilon for leaky integration
  PID pid(kp,ki);
  pid.setDeadband(dp, di);
  pid.seteps(eps);
  size_t cnt = 0;
  int64_t diff_prev = 0;
  ZipAggregator<uint64_t, uint64_t> agg
  (
      [&cnt, &diff_prev, &pid, clip, reject](const uint64_t &a, const uint64_t &b) {
        const int64_t diff = int64_t(b)-int64_t(a);
        const int64_t Delta = (cnt > 0 ? diff_prev-diff : 0);
        const int64_t clippedDelta = (abs(Delta) < clip ? Delta : clip*sgn(Delta));
        const auto control = (abs(Delta) < reject ? pid.update(clippedDelta) : pid.getControl());
        lockcout.lock();
        std::cout << "pair: A=" << a << "  B=" << b << "  diff=" << diff << "  Delta=" << Delta << "  control=" << control << "\n";
        lockcout.unlock();
        cnt++;
        diff_prev = diff;
      },
      /*max_depth_per_queue=*/8
  );
  auto ts_pps  = std::thread(ts_reader, std::ref(input), "PPS", [&ts, &agg, timeout](){
    const auto A = ts.read_with_timeout(timeout);
    agg.submitA(A);
    return A;
  });
  auto ts_sigA = std::thread(ts_reader, std::ref(input), "sigA", [&ts, &agg, timeout](){
    const auto B = ts.readA_with_timeout(timeout);
    agg.submitB(B);
    return B;
  });
  ts_pps.join();
  ts_sigA.join();
  std::cout << "All done, exiting." << std::endl;
  return 0;
}

#include "MCP9808.hh"

int pptemp(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  Args args;
  MCP9808::print_csv_header(args, std::cout);
  MCP9808 sensor(args.bus, args.addr, args.reopen);
  int n = 0;
  while (true) {
    try {
      const double t_c = sensor.read_temp_c();
      std::cout << MCP9808::format_line(args, t_c) << "\n";
      std::cout.flush();
    } catch (const std::system_error& e) {
      if (args.quiet_errors) {
        MCP9808::emit_quiet_error_placeholder(args, std::cout, std::cerr);
      } else {
        throw;
      }
    }
    ++n;
    if (args.count && n >= args.count) break;
    if (args.delay > 0.0)
      std::this_thread::sleep_for(std::chrono::duration<double>(args.delay));
  }
  return 0;
}

int pphelloworld(const InputParser &input, int argc, char *argv[], const Verbosity &v)
{
  FPGA fpga(v);
  streamer s(input, fpga);
  Sequence seq;
  seq.push_back(el(500, ~0).store(0));
  seq.push_back(el(500, 0).store(1));
  seq.push_back(el(Replay{}, 0, 2)); // 0 = repeat indefinitely
  if (v.veryverbose)
    seq.dump(std::cout, "| ");
  s.fifo.send_sequence(seq);
  s.sc.trigger_force();
  std::cout << "All done, exiting. All outputs are toggling with frequency f_clk/1000." << std::endl;
  return 0;
}

int main(int argc, char *argv[])
{
  auto progname = get_program_name(argc, argv);
  about(progname);
  InputParser input(argc, argv);
  auto v = set_verbosity(input);
  check_version(version);
  mlockall(MCL_CURRENT | MCL_FUTURE);
  RealtimeScheduler rt;
  if (v.veryverbose)
    std::cout << "Scheduler: " << rt.report() << std::endl;
  if (v.veryverbose) {
    FPGA fpga(v);
    fpga.mgr.status();
    fpga.status();
  }
  static const std::map<std::string, std::function<int(InputParser&,int,char*[],Verbosity&)>> actions{
    {"pptest", pptest},
    {"ppmstest", ppmstest},
    {"ppdmatest", ppdmatest},
    {"ppfg", ppfg},
    {"ppdelay", ppdelay},
    {"ppreset", ppreset},
    {"pptrig", pptrig},
    {"ppqout", ppqout},
    {"ppaux", ppaux},
    {"ppread", ppread},
    {"ppcounter", ppcounter},
    {"ppts", ppts},
    {"ppgpsdo", ppgpsdo},
    {"pptemp", pptemp},
    {"pphelloworld", pphelloworld}
  };

  int rc = 0;
  if (auto it = actions.find(progname); it != actions.end()) {
    rc = it->second(input, argc, argv, v);
  } else {
    std::cerr << "Unknown program name: " << progname << "\n";
    std::cerr << "Available modes:";
    for (auto const& [name, _] : actions)
      std::cerr << " " << name;
    std::cerr << "\n";
    rc = 1;   // nonzero error code for unknown case
  }

  return rc;
}
