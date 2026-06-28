// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Measurement-oriented `pptool` command implementations.
//
// This file collects the user-facing entry points for subsystems whose main job is
// observation, verification, or environmental control rather than output generation.
// The important pattern is that each command stays thin and delegates the hardware model
// to typed wrappers such as `counter`, `timestamp`, `freq_meter`, and `readback`.

#include <algorithm>
#include <atomic>
#include <exception>
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
#include "address_map.hh"
#include "counter.hh"
#include "delay.hh"
#include "timestamp.hh"
#include "format_with_dispatch.hh"
#include "format.hh"
#include "zip_aggregator.hh"
#include "PID.hh"
#include "DAC.hh"
#include "MCP9808.hh"
#include "freq_meter.hh"
#include "sequence.hh"
#include "sequence_file_format.hh"
#include "elements.hh"
#include "definitions.hh"
#include "basic_multi_dma.hh"
#include "trigger.hh"
#include "readback.hh"
#include "ppworkflow.hh"

std::mutex lockcout;

std::string exception_message(const std::exception_ptr &eptr) {
  if (!eptr)
    return "";
  try {
    std::rethrow_exception(eptr);
  } catch (const std::exception &e) {
    return e.what();
  } catch (...) {
    return "unknown non-standard exception";
  }
}

int timestamp_reader_rc(const std::exception_ptr &eptr) {
  if (!eptr)
    return RC_OK;
  const auto msg = exception_message(eptr);
  if (msg.find("overflow") != std::string::npos)
    return RC_ERROR_OVERFLOW;
  return msg.find("Timeout") != std::string::npos ? RC_TIMEOUT : RC_EXCEPTION;
}

bool timestamp_aggregation_stopped(const std::exception_ptr &eptr) {
  return eptr && exception_message(eptr).find("timestamp aggregation stopped") != std::string::npos;
}

struct TimestampSession {
  timestamp ts;
  double timeout;

  TimestampSession(FPGA &fpga, const InputParser &input, const Verbosity &v) :
    ts(fpga.dev_h2f,
      fpga.dev_lw,
      address_map::h2f::ts_core_pps,
      address_map::h2f::fifo_ts_pps_out, address_map::h2f::fifo_ts_pps_in_csr,
      address_map::h2f::fifo_ts_siga_out, address_map::h2f::fifo_ts_siga_in_csr,
      address_map::lw::pio_cfg),
    timeout(parse_double(input, "-timeout", "0"))
  {
    if (input.exists("-pps_in"))
      ts.sel_pps_in();
    if (input.exists("-pps_xtal"))
      ts.sel_pps_xtal();
    ts.selA(parse_uint32(input, "-selA", "0"));
    if (v.verbose)
      std::cout << "timestamp configuration=" << ts.get_cfg() << std::endl;
  }
};

int ppcounter(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  // Counter workflow:
  //   1. reset the measurement bank
  //   2. optionally generate a built-in reference sequence
  //   3. stream it to the FPGA and force execution
  //   4. latch all counter instruments
  //   5. print reports and optionally run the deterministic self-check
  int rc = RC_OK;
  streamer s(input, fpga);
  counter ctr(input, fpga);
  if (input.exists("-test1"))
    ctr.write_selectors(0, 0, 1);
  ctr.reset_all();
  Sequence seq;
  if (input.exists("-test1"))
    seq = counter_seq1();
  if (input.exists("-test2"))
    seq = counter_seq2(input);
  if (v.veryverbose)
    seq.dump(std::cout, "| ");
  rc |= transmit_sequence_checked(s.fifo, s.sc, seq, v);
  if (rc & RC_TIMEOUT)
    return rc;
  const int trigger_rc = force_trigger_when_output_fifo_ready(s.sc, v);
  rc |= trigger_rc;
  if (trigger_rc != RC_OK)
    return rc;
  rc |= s.sc.wait_to_complete(v);
  if (rc & RC_TIMEOUT)
    return rc;
  ctr.latch_all();
  ctr.report();
  if (input.exists("-test1") && input.exists("-check"))
    if (counter_test1(ctr) != 0)
      rc |= RC_ERROR_CHECK;
  return rc;
}

int ppread(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  // `ppread` is the simplest measurement command: configure output-enable if requested,
  // then stream readback elements until timeout or external termination.
  if (input.exists("-oe")) {
    const bool oe = parse_bool(input, "-oe", "0");
    fpga.output_enable(oe);
  }
  readback rb(input, fpga);
  const auto timeout_policy = readback_timeout_policy(input);
  const bool export_vcd = input.exists("-save-vcd");
  const bool export_text = input.exists("-save-text");
  const bool export_binary = input.exists("-save-binary");
  if (export_vcd || export_text || export_binary) {
    Sequence captured = rb.capture_sequence(timeout_policy);
    const std::string vcd_filename = input.get_string("-save-vcd", "capture.vcd");
    if (export_text)
      write_sequence_to_file(captured, input.get_string("-save-text", "capture.seq"), false);
    if (export_vcd)
      captured.write_VCD_file(vcd_filename);
    if (export_binary)
      captured.write_binary_file(input.get_string("-save-binary", "capture.ppbin"), false);
    std::cout << "Readback capture: size=" << std::dec << captured.size()
              << " length=" << captured.length() << std::endl;
    if (export_text)
      std::cout << "Saved text capture to " << input.get_string("-save-text", "capture.seq") << std::endl;
    if (export_vcd)
      std::cout << "Saved VCD capture to " << vcd_filename << std::endl;
    if (export_binary)
      std::cout << "Saved binary capture to " << input.get_string("-save-binary", "capture.ppbin") << std::endl;
  } else {
    if (v.veryverbose)
      rb.check_fill_status();
    rb.read_all(timeout_policy);
  }
  return RC_OK;
}

void ts_reader(const InputParser &input,
               std::string label,
               std::function<uint64_t()> read,
               long long silent_after = -1,
               std::exception_ptr *failure = nullptr)
{
  // Shared timestamp-printing loop used by both `ppts` and `ppgpsdo`.
  // `silent_after` lets a caller keep collecting data after the initial bring-up logs stop.
  uint64_t ctr = 0;
  uint64_t current = 0;
  uint64_t previous = 0;
  FormatDispatch d;
  d['l'] = [label](std::string_view t) { return setw_l(label, t); };
  d['t'] = [](std::string_view) { return timestamp_iso8601_utc_ms(); };
  d['c'] = [&ctr](std::string_view t) { return setw_l(with_underscores(ctr), t); };
  d['s'] = [&current](std::string_view t) { return setw_l(with_underscores(current), t); };
  d['d'] = [&ctr, &current, &previous](std::string_view t) { return setw_l(ctr ? with_underscores(current-previous) : "", t); };
  d['D'] = [&ctr, &current, &previous](std::string_view t) { return setw_l(ctr ? "diff=" + with_underscores(current-previous) : "", t); };
  std::string fmt = "%4l  %t  ctr=%10c  ts=%15s  %15D";
  try {
    const auto nr = parse_uint64(input, "-nr", "0");
    for (ctr = 0; nr == 0 || ctr < nr; ctr++) {
      current = read();
      if (silent_after < 0 || ctr < static_cast<uint64_t>(silent_after)) {
        std::lock_guard<std::mutex> lock(lockcout);
        std::cout << format_with_dispatch(fmt, d) << std::endl;
      }
      previous = current;
    }
  } catch (...) {
    const auto eptr = std::current_exception();
    if (failure)
      *failure = eptr;
    std::lock_guard<std::mutex> lock(lockcout);
    std::cerr << label << " timestamp reader failed: " << exception_message(eptr) << std::endl;
  }
}

int ppts(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  // Timestamp reader entry point. Depending on the switches, this command can read the
  // PPS path, the auxiliary signal-A path, or both concurrently.
  TimestampSession session(fpga, input, v);
  const bool read_pps = !input.exists("-nopps");
  const bool ignore_ts_overflow = input.exists("-ignore_ts_overflow");
  std::thread ts_pps;
  std::exception_ptr pps_failure;
  if (read_pps)
    ts_pps = std::thread(ts_reader, std::ref(input), "PPS", [&session, ignore_ts_overflow](){ return session.ts.read_with_timeout(session.timeout, ignore_ts_overflow); }, -1, &pps_failure);
  const bool read_sigA = input.exists("-sigA");
  std::thread ts_sigA;
  std::exception_ptr sigA_failure;
  if (read_sigA)
    ts_sigA = std::thread(ts_reader, std::ref(input), "sigA", [&session, ignore_ts_overflow](){ return session.ts.readA_with_timeout(session.timeout, ignore_ts_overflow); }, -1, &sigA_failure);
  if (read_pps) ts_pps.join();
  if (read_sigA) ts_sigA.join();
  return timestamp_reader_rc(pps_failure) | timestamp_reader_rc(sigA_failure);
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
  Averager(size_t _nr, std::function<void(T)> _fnc) : nr(_nr), fnc(std::move(_fnc)) {
    if (nr == 0)
      throw std::runtime_error("Averager window must be greater than zero.");
  }

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
  // GPSDO helper: pair PPS and signal-A timestamps, derive timing error, then feed the
  // averaged error into a PID-controlled DAC output.
  TimestampSession session(fpga, input, v);
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
    {
      std::lock_guard<std::mutex> lock(lockcout);
      std::cout << "avgDelta=" << avgDelta <<
        " control=" << std::setprecision(6) << control <<
        " vout=" << std::setprecision(6) << vout << std::endl;
    }
    dac.set_voltage(vout, vref, gain);
  });
  ZipAggregator<uint64_t, uint64_t> agg
  (
      [&cnt, &diff_prev, clip, reject, very_silent_after, &av](const uint64_t &a, const uint64_t &b) {
        const int64_t diff = int64_t(b)-int64_t(a);
        const int64_t Delta = (cnt > 0 ? diff_prev-diff : 0);
        if (cnt < very_silent_after) {
          std::lock_guard<std::mutex> lock(lockcout);
          std::cout << "pair: A=" << std::dec << a << "  B=" << b << "  diff=" << diff <<
            "  Delta=" << Delta << std::endl;
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
  std::atomic<bool> stop_requested{false};
  auto request_stop = [&]() {
    bool expected = false;
    if (stop_requested.compare_exchange_strong(expected, true))
      agg.stop(false);
  };
  std::exception_ptr pps_failure;
  std::exception_ptr sigA_failure;
  auto ts_pps  = std::thread(ts_reader, std::ref(input), "PPS", [&session, &agg, &stop_requested, &request_stop](){
    if (stop_requested.load())
      throw std::runtime_error("timestamp aggregation stopped");
    try {
      const auto A = session.ts.read_with_timeout(session.timeout, false, [&stop_requested] { return stop_requested.load(); });
      if (!agg.submitA(A))
        throw std::runtime_error("timestamp aggregation stopped");
      return A;
    } catch (...) {
      request_stop();
      throw;
    }
  }, silent_after, &pps_failure);
  auto ts_sigA = std::thread(ts_reader, std::ref(input), "sigA", [&session, &agg, &stop_requested, &request_stop](){
    if (stop_requested.load())
      throw std::runtime_error("timestamp aggregation stopped");
    try {
      const auto B = session.ts.readA_with_timeout(session.timeout, false, [&stop_requested] { return stop_requested.load(); });
      if (!agg.submitB(B))
        throw std::runtime_error("timestamp aggregation stopped");
      return B;
    } catch (...) {
      request_stop();
      throw;
    }
  }, silent_after, &sigA_failure);
  ts_pps.join();
  ts_sigA.join();
  agg.stop(true);
  std::exception_ptr aggregation_failure;
  try {
    agg.rethrow_if_failed();
  } catch (...) {
    aggregation_failure = std::current_exception();
    std::lock_guard<std::mutex> lock(lockcout);
    std::cerr << "GPSDO aggregation failed: " << exception_message(aggregation_failure) << std::endl;
  }
  const bool pps_cancelled = timestamp_aggregation_stopped(pps_failure);
  const bool sigA_cancelled = timestamp_aggregation_stopped(sigA_failure);
  const bool real_failure = aggregation_failure ||
    (pps_failure && !pps_cancelled) ||
    (sigA_failure && !sigA_cancelled);
  const int pps_rc = (pps_cancelled && real_failure) ? RC_OK : timestamp_reader_rc(pps_failure);
  const int sigA_rc = (sigA_cancelled && real_failure) ? RC_OK : timestamp_reader_rc(sigA_failure);
  return pps_rc | sigA_rc | timestamp_reader_rc(aggregation_failure);
}

int pptemp(FPGA &, const InputParser &input, const Verbosity &)
{
  // Temperature polling loop for the MCP9808 sensor. The formatting and retry policy live
  // in the sensor wrapper so this command can stay focused on the read/print cadence.
  Args args;
  try {
    const auto bus = parse_uint32(input, "-bus", "1");
    if (bus > static_cast<uint32_t>(std::numeric_limits<int>::max()))
      throw std::runtime_error("-bus is out of range");
    args.bus = static_cast<int>(bus);
    const auto addr = parse_uint32(input, "-addr", "0x18");
    if (addr < 0x03 || addr > 0x77)
      throw std::runtime_error("-addr must be a 7-bit I2C address in range 0x03..0x77");
    args.addr = static_cast<int>(addr);
    args.delay = parse_time(input, "-wait", "1s");
    args.count = parse_uint64(input, "-nr", "0");
    args.celsius = true; // default; -celsius is accepted for explicitness.
    if (input.exists("-celsius"))
      args.celsius = true;
    args.fahrenheit = input.exists("-fahrenheit");
    args.kelvin = input.exists("-kelvin");
    args.timestamp = input.exists("-timestamp");
    args.csv = input.exists("-csv");
    args.reopen = input.exists("-reopen");
    args.quiet_errors = input.exists("-quiet-errors");
  } catch (const std::exception &e) {
    std::cerr << "Invalid pptemp argument: " << e.what() << std::endl;
    return RC_INVALID_ARG;
  }
  MCP9808::print_csv_header(args, std::cout);
  MCP9808 sensor(args.bus, args.addr, args.reopen);
  uint64_t n = 0;
  while (true) {
    try {
      const double t_c = sensor.read_temp_c();
      std::cout << MCP9808::format_line(args, t_c) << "\n";
      std::cout.flush();
    } catch (const std::system_error& e) {
      if (args.quiet_errors) {
        MCP9808::emit_quiet_error_placeholder(args, std::cout, std::cerr, e.what());
      } else {
        throw;
      }
    }
    ++n;
    if (args.count && n >= args.count) break;
    if (args.delay > 0.0) sleepd(args.delay);
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
  int rc = transmit_sequence_checked(s.fifo, s.sc, seq, v);
  if (rc != RC_OK)
    return rc;
  rc |= force_trigger_when_output_fifo_ready(s.sc, v);
  if (rc != RC_OK)
    return rc;
  std::cout << "All outputs are now toggling with frequency f_clk/1000." << std::endl;
  return RC_OK;
}

int ppfreq(FPGA &fpga, const InputParser &input, const Verbosity &v) {
  pp_freq_meter fm(input, fpga, false, v.veryverbose);
  if (input.exists("-gate_time")) {
    auto gate_time = parse_time(input, "-gate_time", "1s");
    fm.meter.set_gate_time(gate_time);
  } else {
    auto gate_len = parse_uint32(input, "-gate_len", "500000");
    fm.meter.set_gate_len(gate_len);
  }
  uint64_t ctr;
  FormatDispatch d;
  d['t'] = [](std::string_view) { return timestamp_iso8601_utc_ms(); };
  d['c'] = [&ctr](std::string_view t) { return setw_l(with_underscores(ctr), t); };
  d['e'] = [&fm](std::string_view t) { return setw_l(fm.meter.read_freq_str(0), t); };
  d['i'] = [&fm](std::string_view t) { return setw_l(fm.meter.read_freq_str(1), t); };
  d['s'] = [&fm](std::string_view t) { return setw_l(fm.meter.read_freq_str(2), t); };
  std::string fmt = input.get_string("-format", "%t %e");
  const auto nr = parse_uint64(input, "-nr", "0");
  for (ctr = 0; nr == 0 || ctr < nr; ctr++) {
    fm.meter.wait_one_gate_time();
    fm.refresh_streamer_clk();
    std::cout << format_with_dispatch(fmt, d) << std::endl;
  }
  return RC_OK;
}

std::pair<Sequence, bool> load_sequence_from_file(const InputParser &input,
                                                  const std::string &filename,
                                                  const SequenceFileFormat format)
{
  switch (format) {
    case SequenceFileFormat::vcd: {
      try {
        Sequence seq;
        const std::string target_name = input.get_string("-target", "outs");
        const auto scale_factor = parse_vcd_scale_factor(input);
        seq.load_VCD(filename, target_name, scale_factor);
        const bool force_now = input.exists("-force") ? force_trigger : do_not_force_trigger;
        return {seq, force_now};
      }
      catch (const std::exception &e) {
        throw std::runtime_error("Error loading VCD file '" + filename + "': " + e.what());
      }
    }
    case SequenceFileFormat::text: {
      try {
        std::ifstream f(filename);
        if (!f)
          throw std::runtime_error("Could not open sequence file");
        auto [seq, force_trigger] = parse_sequence_from_stream(f);
        if (input.exists("-force"))
          force_trigger = true;
        return {seq, force_trigger};
      }
      catch (const std::exception &e) {
        throw std::runtime_error("Error parsing sequence file '" + filename + "': " + e.what());
      }
    }
    case SequenceFileFormat::binary:
      try {
        auto [seq, force_trigger] = Sequence::read_binary_file(filename);
        if (input.exists("-force"))
          force_trigger = true;
        return {seq, force_trigger};
      }
      catch (const std::exception &e) {
        throw std::runtime_error("Error reading binary sequence file '" + filename + "': " + e.what());
      }
  }
  throw std::runtime_error("Unhandled sequence file format");
}

int play_loaded_sequence(FPGA &fpga,
                        const InputParser &input,
                        const Verbosity &v,
                        Sequence seq,
                        const bool force_trigger)
{
  streamer s(input, fpga);
  trigger tr(input, fpga);
  readback rb(input, fpga);
  counter ctr(input, fpga);
  ctr.reset_all();
  return send_and_trig(s.fifo, s.sc, rb, ctr, seq, input, force_trigger, v);
}

int ppplay(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  try {
    const std::string filename = input.get_string("-file", "");
    if (filename.empty()) {
      std::cerr << "Missing required -file argument." << std::endl;
      return RC_INVALID_ARG;
    }
    const auto format = resolve_sequence_file_format(input, filename);
    validate_sequence_file_options(input, format);
    auto [seq, force_now] = load_sequence_from_file(input, filename, format);
    if (v.verbose) {
      std::cout << "ppplay: file=" << filename
                << " format=" << (format == SequenceFileFormat::vcd ? "vcd" : format == SequenceFileFormat::text ? "text" : "binary")
                << " force_trigger=" << (force_now ? "true" : "false") << std::endl;
    }
    if (v.veryverbose)
      seq.dump(std::cout, "| ");
    return play_loaded_sequence(fpga, input, v, std::move(seq), force_now);
  }
  catch (const std::exception &e) {
    std::cout << "exception: " << e.what() << std::endl;
    return RC_EXCEPTION;
  }
}

int ppvcd(FPGA &fpga, const InputParser &input, const Verbosity &v)
{
  try {
    const std::string filename = input.get_string("-file", "");
    if (filename == "") {
      std::cerr << "Missing required -file argument." << std::endl;
      return RC_INVALID_ARG;
    }
    const auto format = resolve_sequence_file_format(input, filename, SequenceFileFormat::vcd);
    if (format != SequenceFileFormat::vcd)
      throw std::runtime_error("ppvcd only supports VCD input. Use ppplay for other formats.");
    validate_sequence_file_options(input, format);
    auto [seq, force_now] = load_sequence_from_file(input, filename, format);
    if (v.verbose)
      std::cout << "ppvcd: compatibility alias for ppplay -format vcd" << std::endl;
    if (v.veryverbose)
      seq.dump(std::cout, "| ");
    return play_loaded_sequence(fpga, input, v, std::move(seq), force_now);
  }
  catch (const std::exception &e) {
    std::cout << "exception: " << e.what() << std::endl;
    return RC_EXCEPTION;
  }
  return RC_OK;
}
