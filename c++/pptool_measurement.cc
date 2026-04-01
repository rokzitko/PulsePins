// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "pptool_commands.hh"
#include "counter.hh"
#include "timestamp.hh"
#include "format_with_dispatch.hh"
#include "format.hh"
#include "zip_aggregator.hh"
#include "PID.hh"
#include "DAC.hh"
#include "MCP9808.hh"
#include "freq_meter.hh"
#include "sequence.hh"
#include "elements.hh"
#include "definitions.hh"
#include "basic_multi_dma.hh"
#include "trigger.hh"
#include "readback.hh"
#include "ppworkflow.hh"

std::mutex lockcout;
#define LOCKCOUT(z) { lockcout.lock(); z; lockcout.unlock(); }

int ppcounter(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  int rc = 0;
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
  return rc;
}

int ppread(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  if (input.exists("-oe")) {
    const bool oe = parse_bool(input, "-oe", "0");
    fpga.output_enable(oe);
  }
  readback rb(input, fpga);
  if (v.veryverbose)
    rb.check_fill_status();
  const double timeout = readback_timeout(input);
  rb.read_all(timeout);
  return RC_OK;
}

void ts_reader(const InputParser &input, std::string label, std::function<uint64_t()> read, long long silent_after = -1)
{
  long long ctr = 0;
  uint64_t current = 0;
  uint64_t previous = 0;
  FormatDispatch d;
  d['l'] = [label](std::string_view t) { return setw_l(label, t); };
  d['t'] = [](std::string_view t) { return timestamp_iso8601_utc_ms(); };
  d['c'] = [&ctr](std::string_view t) { return setw_l(with_underscores(ctr), t); };
  d['s'] = [&current](std::string_view t) { return setw_l(with_underscores(current), t); };
  d['d'] = [&ctr, &current, &previous](std::string_view t) { return setw_l(ctr ? with_underscores(current-previous) : "", t); };
  d['D'] = [&ctr, &current, &previous](std::string_view t) { return setw_l(ctr ? "diff=" + with_underscores(current-previous) : "", t); };
  std::string fmt = "%4l  %t  ctr=%10c  ts=%15s  %15D";
  try {
    const long long nr = parse_uint64(input, "-nr", "0");
    for (ctr = 0; nr == 0 || ctr <= nr; ctr++) {
      current = read();
      if (silent_after < 0 || ctr < silent_after)
        LOCKCOUT( std::cout << format_with_dispatch(fmt, d) << std::endl; )
      previous = current;
    }
  } catch (const std::runtime_error &e) {
    std::cout << "Exception: " << e.what() << std::endl;
  }
}

int ppts(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  rstmgr rm;
  rm.s2f_reset();
  timestamp ts(fpga.dev_h2f,
               fpga.dev_lw,
               FIFO_TS_PPS_OUT_BASE, FIFO_TS_PPS_IN_CSR_BASE,
               FIFO_TS_SIGA_OUT_BASE, FIFO_TS_SIGA_IN_CSR_BASE,
               PIO_CFG_BASE);
  const double timeout = parse_double(input, "-timeout", "0");
  const bool read_pps = !input.exists("-nopps");
  if (input.exists("-pps_in"))
    ts.sel_pps_in();
  if (input.exists("-pps_xtal"))
    ts.sel_pps_xtal();
  std::thread ts_pps;
  if (read_pps)
    ts_pps = std::thread(ts_reader, std::ref(input), "PPS", [&ts, timeout](){ return ts.read_with_timeout(timeout); }, -1);
  const bool read_sigA = input.exists("-sigA");
  const auto selA = parse_uint32(input, "-selA", "0");
  ts.selA(selA);
  if (v.verbose)
    std::cout << "timestamp configuration=" << ts.get_cfg() << std::endl;
  std::thread ts_sigA;
  if (read_sigA)
    ts_sigA = std::thread(ts_reader, std::ref(input), "sigA", [&ts, timeout](){ return ts.readA_with_timeout(timeout); }, -1);
  if (read_pps) ts_pps.join();
  if (read_sigA) ts_sigA.join();
  return RC_OK;
}

template <typename T> int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

class linear {
 protected:
  double k = 0.0;
  double l = 0.0;
  double ymin = -std::numeric_limits<double>::max();
  double ymax = std::numeric_limits<double>::max();

 public:
  linear() {}

  linear(double _k, double _l) {
    k = _k; l = _l;
  }

  linear (double _k, double _l, double _ymin, double _ymax) {
    k = _k; l = _l; ymin = _ymin; ymax = _ymax;
  }

  double y(const double x) {
    double y = k+l*x;
    return std::clamp(y, ymin, ymax);
  }

  std::string settings() {
    std::stringstream s;
    s << "Linear model, y=k*x+l: k=" << k << " l=" << l;
    if (ymin != -std::numeric_limits<double>::max()) s << "; ymin=" << ymin;
    if (ymax != +std::numeric_limits<double>::max()) s << "; ymax=" << ymax;
    return s.str();
  }
};

class dac_linear : public linear {
 public:
  void parse(const InputParser &input) {
    k = parse_double(input, "-k", "2.6");
    l = parse_double(input, "-l", "-0.01");
    ymin = parse_double(input, "-vmin", "0.0");
    ymax = parse_double(input, "-vmax", "5.0");
  }
};

template <typename T>
class Averager {
 private:
  T sum {0};
  size_t ctr {0};
  size_t nr {0};
  std::function<void(T)> fnc;

 public:
  Averager(size_t _nr, std::function<void(T)> _fnc) : nr(_nr), fnc(std::move(_fnc)) {}

  void add(T x) {
    sum += x;
    ctr++;
    if (ctr == nr) {
      T avg = sum/nr;
      fnc(avg);
      sum = 0;
      ctr = 0;
    }
  }
};

int ppgpsdo(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  rstmgr rm;
  rm.s2f_reset();
  timestamp ts(fpga.dev_h2f,
               fpga.dev_lw,
               FIFO_TS_PPS_OUT_BASE, FIFO_TS_PPS_IN_CSR_BASE,
               FIFO_TS_SIGA_OUT_BASE, FIFO_TS_SIGA_IN_CSR_BASE,
               PIO_CFG_BASE);
  const double timeout = parse_double(input, "-timeout", "0");
  if (input.exists("-pps_in"))
    ts.sel_pps_in();
  if (input.exists("-pps_xtal"))
    ts.sel_pps_xtal();
  const auto selA = parse_uint32(input, "-selA", "0");
  ts.selA(selA);
  if (v.verbose)
    std::cout << "timestamp configuration=" << ts.get_cfg() << std::endl;
  const auto kp = parse_double(input, "-kp", "0.01");
  const auto ki = parse_double(input, "-ki", "0.1");
  const int64_t clip = parse_uint64(input, "-clip", "1000");
  const int64_t reject = parse_uint64(input, "-reject", "10000");
  const auto dp = parse_double(input, "-dp", "0");
  const auto di = parse_double(input, "-di", "0");
  const auto eps = parse_double(input, "-eps", "0.0");
  const size_t silent_after = 20;
  const size_t very_silent_after = 100;
  PID pid(kp,ki);
  pid.setDeadband(dp, di);
  pid.seteps(eps);
  if (v.verbose) std::cout << pid.settings() << std::endl;
  const int bus = 1;
  I2CDevice dev(bus);
  const uint8_t addr = 0x4C;
  AD5693 dac(dev, addr);
  const int gain = 2;
  const bool disable_ref = false;
  const double vref = 2.5;
  dac.write_control(gain == 2, disable_ref);
  dac.set_voltage(2.6, vref, gain);
  dac_linear convert;
  convert.parse(input);
  if (v.verbose) std::cout << convert.settings() << std::endl;
  size_t cnt = 0;
  int64_t diff_prev = 0;
  const size_t avg = parse_uint32(input, "-avg", "1");
  Averager<double> av(avg, [&pid, &convert, &dac, vref, gain](double avgDelta) {
    const auto control = pid.update(avgDelta);
    const double vout = convert.y(control);
    lockcout.lock();
    std::cout << "avgDelta=" << avgDelta <<
      " control=" << std::setprecision(6) << control <<
      " vout=" << std::setprecision(6) << vout << std::endl;
    lockcout.unlock();
    dac.set_voltage(vout, vref, gain);
  });
  ZipAggregator<uint64_t, uint64_t> agg
  (
      [&cnt, &diff_prev, clip, reject, very_silent_after, &av](const uint64_t &a, const uint64_t &b) {
        const int64_t diff = int64_t(b)-int64_t(a);
        const int64_t Delta = (cnt > 0 ? diff_prev-diff : 0);
        if (cnt < very_silent_after) {
          lockcout.lock();
          std::cout << "pair: A=" << std::dec << a << "  B=" << b << "  diff=" << diff <<
            "  Delta=" << Delta << std::endl;
          lockcout.unlock();
        }
        if (cnt > 0 && abs(Delta) < reject) {
          const int64_t clippedDelta = (abs(Delta) < clip ? Delta : clip*sgn(Delta));
          av.add(clippedDelta);
        }
        cnt++;
        diff_prev = diff;
      },
      8
  );
  auto ts_pps  = std::thread(ts_reader, std::ref(input), "PPS", [&ts, &agg, timeout](){
    const auto A = ts.read_with_timeout(timeout);
    agg.submitA(A);
    return A;
  }, silent_after);
  auto ts_sigA = std::thread(ts_reader, std::ref(input), "sigA", [&ts, &agg, timeout](){
    const auto B = ts.readA_with_timeout(timeout);
    agg.submitB(B);
    return B;
  }, silent_after);
  ts_pps.join();
  ts_sigA.join();
  return RC_OK;
}

int pptemp(FPGA &fpga, const InputParser &input, const Verbosity &v)
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
    if (args.delay > 0.0) sleep(args.delay);
  }
  return RC_OK;
}

int pphelloworld(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  streamer s(input, fpga);
  Sequence seq;
  seq.push_back(el(500, ~0).store(0));
  seq.push_back(el(500, 0).store(1));
  seq.push_back(el(Replay{}, 0, 2));
  if (v.veryverbose)
    seq.dump(std::cout, "| ");
  s.fifo.send_sequence(seq);
  s.sc.trigger_force();
  std::cout << "All outputs are now toggling with frequency f_clk/1000." << std::endl;
  return RC_OK;
}

int ppfreq(FPGA &fpga, const InputParser &input, const Verbosity &v) {
  pp_freq_meter fm(input, fpga, false);
  if (input.exists("-gate_time")) {
    auto gate_time = parse_time(input, "-gate_time", "1s");
    fm.meter.set_gate_time(gate_time);
  } else {
    auto gate_len = parse_uint32(input, "-gate_len", "500000");
    fm.meter.set_gate_len(gate_len);
  }
  size_t ctr;
  FormatDispatch d;
  d['t'] = [](std::string_view t) { return timestamp_iso8601_utc_ms(); };
  d['c'] = [&ctr](std::string_view t) { return setw_l(with_underscores(ctr), t); };
  d['e'] = [&fm](std::string_view t) { return setw_l(fm.meter.read_freq_str(0), t); };
  d['i'] = [&fm](std::string_view t) { return setw_l(fm.meter.read_freq_str(1), t); };
  d['s'] = [&fm](std::string_view t) { return setw_l(fm.meter.read_freq_str(2), t); };
  std::string fmt = "%t %e";
  const long long nr = parse_uint64(input, "-nr", "0");
  for (ctr = 0; ctr <= nr || nr == 0; ctr++) {
    fm.meter.wait_one_gate_time();
    std::cout << format_with_dispatch(fmt, d) << std::endl;
  }
  return RC_OK;
}

int ppvcd(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  try {
    streamer s(input, fpga);
    trigger tr(input, fpga);
    readback rb(input, fpga);
    counter ctr(input, fpga);
    ctr.reset_all();
    const std::string filename = input.get_string("-file", "");
    if (filename == "") {
      std::cout << "Specify filename using -file." << std::endl;
      return 0;
    }
    const std::string target_name = input.get_string("-target", "outs");
    const auto scale_factor = input.get_uint32("-scale", 10);
    const bool trig = input.exists("-force") ? force_trigger : do_not_force_trigger;
    if (v.verbose)
      std::cout << "Loading sequence from " << filename << " target=" << target_name << " scale=" << scale_factor << std::endl;
    Sequence elements;
    elements.load_VCD(filename, target_name, scale_factor);
    return send_and_trig(s.fifo, s.sc, rb, ctr, elements, input, trig, v);
  }
  catch (const char *e) {
    std::cout << "exception: " << e << std::endl;
  }
  return 0;
}
