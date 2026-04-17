// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

// Shared execution helpers for commands that stream a Sequence into the FPGA.
//
// `send_and_trig(...)` is the common high-level workflow used by several tools:
//   - append the expected final output value
//   - transmit the sequence through the chosen transport
//   - arm or force the trigger
//   - optionally validate the readback stream
//   - wait for completion and check final-state invariants
//   - report FIFO, CRC, readback, and counter status
//
// The goal is consistent behavior across CLI entry points, so changes here should be
// treated as policy changes for multiple commands, not as a local helper tweak.

#include <iostream>
#include <iomanip>
#include <optional>

#include "colors.hh"
#include "format.hh"
#include "misc.hh"
#include "delay.hh"
#include "parser.hh"
#include "verbosity.hh"
#include "elements.hh"
#include "sequence.hh"
#include "streamer_control.hh"
#include "sequences.hh"
#include "readback.hh"
#include "counter.hh"
#include "definitions.hh"

inline void identity([[maybe_unused]] Sequence &e) {
}

template <std::size_t nr>
inline void drop(Sequence &s)
{
  if (nr < s.size())
    s.erase(s.begin() + nr);
}

inline void drop_count0(Sequence &s)
{
  for (auto it = s.begin(); it != s.end(); ) {
    if (it->count() == 0) {
      it = s.erase(it);
    } else {
      ++it;
    }
  }
}

inline void convert_for_readback_check(Sequence &s) {
  auto s_new = s.convert_to_BitLoad().merge();
  s = s_new;
}

inline auto readback_timeout(const InputParser &input) {
  double timeout = 0.0;
  if (input.exists("-timeout")) {
    timeout = parse_double(input, "-timeout", "0");
    std::cout << "readback timeout=" << abs(timeout) << (timeout > 0 ? "s [after last read]" : "s [after start]") << std::endl;
  }
  return timeout;
}

inline std::optional<value_t> explicit_final_output(const Sequence &elements)
{
  if (!elements.empty() && elements.back().is_final())
    return elements.back().value();
  return std::nullopt;
}

inline value_t append_final_output(Sequence &elements, const InputParser &input)
{
  if (const auto explicit_final = explicit_final_output(elements)) {
    if (input.exists("-t"))
      throw std::runtime_error("Sequence already contains an explicit final output; omit -t or remove the final record");
    return *explicit_final;
  }
  const value_t final = input.exists("-t") ? parse_value(input, "-t", "0") : random_value();
  elements.push_back(el(final));
  return final;
}

inline std::pair<Sequence, value_t> prepare_sequence_for_streaming(const Sequence &elements,
                                                                   const InputParser &input)
{
  Sequence prepared = elements;
  const value_t final = append_final_output(prepared, input);
  return {prepared, final};
}

inline void dump_sequence(const Sequence &elements, const Verbosity &v)
{
  if (v.veryverbose)
    elements.dump(std::cout, "| ");
}

template<typename Transport>
inline void transmit_sequence(Transport &tr,
                              streamer_control &sc,
                              Sequence &elements,
                              const Verbosity &v)
{
  if (v.veryverbose)
    tr.report();
  if (elements.size() > max_size)
    std::cout << red << "Sequence will not fit in buffers. size=" << elements.size() << " max_size=" << max_size << rst << std::endl;
  tr.send_sequence(elements);
  sc.status_report();
  if (v.veryverbose)
    tr.report();
}

inline void activate_trigger(streamer_control &sc,
                            const InputParser &input,
                            const bool force_trigger,
                            const Verbosity &v)
{
  if (force_trigger) {
    if (v.verbose)
      std::cout << cyan << " ---> Forcing trigger." << rst << std::endl;
    if (input.exists("-delay")) {
      const double delay = parse_time(input, "-delay", "0");
      sleep(delay);
    }
    sc.trigger_force();
    sc.status_report();
  } else {
    if (v.verbose)
      std::cout << cyan << " --->  Arming trigger." << rst << std::endl;
    sc.trigger_enable();
    sc.status_report();
  }
}

inline void deactivate_trigger(streamer_control &sc,
                               const bool force_trigger,
                               const Verbosity &v)
{
  if (force_trigger) {
    if (v.verbose)
      std::cout << cyan << " ---> Clearing trigger." << rst << std::endl;
    sc.trigger_clear();
    sc.status_report();
  } else {
    if (v.verbose)
      std::cout << cyan << " --->  Disarming trigger." << rst << std::endl;
    sc.trigger_disable();
    sc.status_report();
  }
}

template<typename Convert>
inline bool run_readback_check_phase(readback &rb,
                                    Sequence &elements,
                                    const InputParser &input,
                                    const Verbosity &v,
                                    Convert convert,
                                    const double timeout,
                                    int &rc)
{
  bool rb_failure = false;
  if (input.exists("-check")) {
    // Readback comparison is done against the effective output stream, not necessarily
    // against the original input representation. `convert(...)` lets callers normalize
    // features such as non-BITLOAD updates before the check.
    convert(elements);
    drop_count0(elements);
    if (v.veryverbose && input.exists("-dump-converted"))
      elements.dump(std::cout, "% ");
    if (v.veryverbose)
      rb.check_fill_status();
    const auto successful = rb.check(elements, timeout);
    if (!successful) {
      rb_failure = true;
      rc |= RC_ERROR_CHECK;
    }
  }
  return rb_failure;
}

inline void run_readback_dump_phase(readback &rb,
                                    const InputParser &input,
                                    const Verbosity &v,
                                    const double timeout)
{
  if (input.exists("-read")) {
    if (v.veryverbose)
      rb.check_fill_status();
    rb.read_all(timeout);
  }
}

inline int run_post_execution_checks(streamer_control &sc,
                                    readback &rb,
                                    counter &ctr,
                                    const value_t final,
                                    const bool rb_failure,
                                    const bool force_trigger,
                                    const InputParser &input,
                                    const Verbosity &v,
                                    int rc)
{
  // The remaining checks are post-completion invariants for the whole streamer path.
  sc.wait_to_complete(v);
  sleep_1ms();
  value_t final_qout = sc.get_qout();
  const bool match = final_qout == final;
  std::cout << "send_and_trig(): Final qout=" << hex8(final_qout) << " [" << dec13(final_qout) << "]";
  if (match) {
    std::cout << " OK" << std::endl;
  } else {
    if (!(envVarExists("PP_IGNORE_QOUT_FINAL") || input.exists("-pp_ignore_qout_final"))) {
      std::cout << red << " Mismatch: expecting " << hex8(final) << rst << std::endl;
      rc |= RC_ERROR_CHECK;
    }
  }
  sc.statistics();
  if (sc.get_input_fifo1_ctr_in() != sc.get_input_fifo1_ctr_out()) {
    std::cout << red << "Mismatch in the streamer input FIFO1 detected." << rst << std::endl;
    rc |= RC_ERROR_CHECK;
  }
  if (sc.get_input_fifo2_ctr_in() != sc.get_input_fifo2_ctr_out()) {
    std::cout << red << "Mismatch in the streamer input FIFO2 detected." << rst << std::endl;
    rc |= RC_ERROR_CHECK;
  }
  if (sc.get_output_fifo_ctr_in() != sc.get_output_fifo_ctr_out()) {
    std::cout << red << "Mismatch in the streamer output FIFO detected." << rst << std::endl;
    rc |= RC_ERROR_CHECK;
  }
  if (sc.get_overflow()) {
    std::cout << red << "Input FIFO overflow detected." << rst << std::endl;
    rc |= RC_ERROR_OVERFLOW;
  }
  if (rb.overflow()) {
    std::cout << red << "Readback overflow detected." << rst << std::endl;
    rc |= RC_ERROR_OVERFLOW;
  }
  ctr.latch_all();
  ctr.short_report();
  if (sc.buffer_error()) {
    std::cout << red << "Buffer error detected." << rst << std::endl;
    rc |= RC_ERROR_BUFFER_ERROR;
  }
  const auto crc32sc = sc.get_crc32();
  const auto crc32rb = rb.get_crc32();
  std::cout << "send_and_trig(): CRC=0x" << std::hex << std::setw(8) << std::setfill('0') << crc32sc;
  const bool crcOK = crc32sc == crc32rb;
  if (crcOK) {
    std::cout << green << " OK" << rst << std::endl;
  } else {
    std::cout << red << " Mismatch in readback CRC. Got=0x" << std::hex << std::setw(8) << std::setfill('0') << crc32rb << rst << std::endl;
    rc |= RC_ERROR_CRC_MISMATCH;
  }
  if (crcOK && rb_failure)
    if (input.exists("-ignore_rb_error_if_crc_ok") || envVarExists("PP_IGNORE_RB_ERROR_IF_CRC_OK"))
      rc &= ~RC_ERROR_CHECK;
  deactivate_trigger(sc, force_trigger, v);
  return rc;
}

template<typename Transport, typename Convert>
inline int send_and_trig(Transport &tr,
                        streamer_control &sc,
                        readback &rb,
                        counter &ctr,
                        Sequence &elements,
                        const InputParser &input,
                        const bool force_trigger,
                        const Verbosity &v,
                        Convert convert)
{
  // Contract:
  // - `elements` is the caller-owned requested sequence and is left unchanged.
  // - a working copy is prepared so the post-run qout state can be checked without mutating
  //   cached text/binary/SCPI sequences.
  // - if `-check` is enabled, only that working copy may be converted into a readback-friendly
  //   form before comparison.
  // - the return code accumulates multiple error bits instead of stopping at first failure.
  int rc = RC_OK;
  auto [working_elements, final] = prepare_sequence_for_streaming(elements, input);
  dump_sequence(working_elements, v);
  transmit_sequence(tr, sc, working_elements, v);
  activate_trigger(sc, input, force_trigger, v);

  const double timeout = readback_timeout(input);
  bool rb_failure = run_readback_check_phase(rb, working_elements, input, v, convert, timeout, rc);
  run_readback_dump_phase(rb, input, v, timeout);

  if (input.exists("-dont_wait"))
    return rc;

  return run_post_execution_checks(sc, rb, ctr, final, rb_failure, force_trigger, input, v, rc);
}

template<typename Transport>
inline int send_and_trig(Transport &tr, streamer_control &sc, readback &rb, counter &ctr,
                        Sequence &elements, const InputParser &input, const bool force_trigger, const Verbosity &v) {
  return send_and_trig(tr, sc, rb, ctr, elements, input, force_trigger, v, identity);
}

constexpr bool force_trigger = true;
constexpr bool do_not_force_trigger = false;
