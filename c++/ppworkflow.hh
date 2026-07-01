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

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "colors.hh"
#include "format.hh"
#include "misc.hh"
#include "delay.hh"
#include "parser.hh"
#include "verbosity.hh"
#include "elements.hh"
#include "sequence.hh"
#include "stall_timeout.hh"
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

inline void convert_for_readback_check(Sequence &s, const value_t initial_value = 0) {
  auto s_new = s.convert_to_BitLoad(initial_value).merge();
  s = std::move(s_new);
}

template<typename Convert>
inline void convert_readback_reference(Convert convert, Sequence &elements, const value_t initial_value) {
  if constexpr (std::is_invocable_v<Convert, Sequence&, value_t>) {
    convert(elements, initial_value);
  } else {
    convert(elements);
  }
}

inline constexpr double default_readback_first_element_timeout_s = 2.0;
inline constexpr double default_readback_idle_timeout_s = 2.0;
inline constexpr uint64_t default_output_fifo_ready_timeout_ms = 2'000;
inline constexpr const char *output_fifo_ready_timeout_text =
  "timed out waiting for streamer output FIFO data before force trigger";

inline ReadbackTimeoutPolicy readback_timeout_policy(const InputParser &input) {
  const bool has_timeout = input.exists("-timeout");
  const bool has_hard_timeout = input.exists("-hard-timeout");
  if (!has_timeout && !has_hard_timeout) {
    return {default_readback_first_element_timeout_s, default_readback_idle_timeout_s, 0.0};
  }

  double idle_timeout = 0.0;
  double hard_timeout = 0.0;
  bool timeout_disabled = false;

  if (has_timeout) {
    const double timeout = parse_double(input, "-timeout", "0");
    if (timeout == 0.0) {
      timeout_disabled = true;
    } else if (timeout > 0.0) {
      idle_timeout = timeout;
    } else {
      if (has_hard_timeout)
        throw std::runtime_error("Use either negative -timeout or -hard-timeout, not both.");
      hard_timeout = std::abs(timeout);
    }
  }

  if (has_hard_timeout) {
    hard_timeout = parse_time(input, "-hard-timeout", "0");
    if (hard_timeout <= 0.0)
      throw std::runtime_error("-hard-timeout must be greater than zero.");
  }

  if (timeout_disabled && idle_timeout == 0.0 && hard_timeout == 0.0) {
    std::cout << "readback timeout disabled" << std::endl;
    return {};
  }
  if (idle_timeout > 0.0)
    std::cout << "readback timeout=" << idle_timeout << "s [after last read]" << std::endl;
  if (hard_timeout > 0.0)
    std::cout << "readback hard-timeout=" << hard_timeout << "s [after start]" << std::endl;
  return {0.0, idle_timeout, hard_timeout};
}

inline std::optional<value_t> explicit_final_output(const Sequence &elements)
{
  elements.validate_final_is_terminal();
  if (!elements.empty() && elements.back().is_final())
    return elements.back().value();
  return std::nullopt;
}

inline bool random_final_requested(const InputParser &input)
{
  return input.exists("-random_final") || envVarExists("PP_RANDOM_FINAL");
}

inline el no_modify_final_output()
{
  return el::final_with_value(BitOr(0));
}

inline std::optional<value_t> infer_sequence_final_output(const Sequence &elements, value_t initial_value)
{
  elements.validate_final_is_terminal();
  value_t current = initial_value;
  for (const auto &e : elements) {
    if (e.is_regular()) {
      if (e.count() != 0)
        current = e.updated_value(current);
    } else if (e.is_trigger()) {
      continue;
    } else if (e.is_final()) {
      return e.updated_value(current);
    } else {
      return std::nullopt;
    }
  }
  return current;
}

inline std::optional<value_t> append_final_output(Sequence &elements,
                                                 const InputParser &input,
                                                 const value_t initial_value = 0)
{
  const bool use_t = input.exists("-t");
  const bool use_random = random_final_requested(input);
  if (use_t && use_random)
    throw std::runtime_error("Specify only one final output policy: -t, -random_final, or PP_RANDOM_FINAL");

  if (explicit_final_output(elements)) {
    if (use_t || use_random)
      throw std::runtime_error("Sequence already contains an explicit final output; omit -t/-random_final/PP_RANDOM_FINAL or remove the final record");
    return infer_sequence_final_output(elements, initial_value);
  }

  if (use_t) {
    const value_t final = parse_value(input, "-t", "0");
    elements.push_back(el(final));
    return final;
  }

  if (use_random) {
    const value_t final = random_value();
    elements.push_back(el(final));
    return final;
  }

  elements.push_back(no_modify_final_output());
  return infer_sequence_final_output(elements, initial_value);
}

inline std::pair<Sequence, std::optional<value_t>> prepare_sequence_for_streaming(const Sequence &elements,
                                                                                  const InputParser &input,
                                                                                  const value_t initial_value = 0)
{
  Sequence prepared = elements;
  const auto final = append_final_output(prepared, input, initial_value);
  return {prepared, final};
}

inline void dump_sequence(const Sequence &elements, const Verbosity &v)
{
  if (v.veryverbose)
    elements.dump(std::cout, "| ");
}

template<typename Transport, typename StreamerControl>
inline void transmit_sequence(Transport &tr,
                              StreamerControl &sc,
                              const Sequence &elements,
                              const Verbosity &v)
{
  if (v.veryverbose)
    tr.report();
  if (elements.size() > max_size)
    throw std::runtime_error("Sequence will not fit in buffers. size=" + std::to_string(elements.size()) +
                             " max_size=" + std::to_string(max_size));
  tr.send_sequence(elements);
  sc.status_report();
  if (v.veryverbose)
    tr.report();
}

template<typename Transport, typename StreamerControl>
inline int transmit_sequence_checked(Transport &tr,
                                     StreamerControl &sc,
                                     const Sequence &elements,
                                     const Verbosity &v,
                                     std::ostream &out = std::cout)
{
  try {
    transmit_sequence(tr, sc, elements, v);
    return RC_OK;
  } catch (const StallTimeout &e) {
    out << red << e.what() << rst << std::endl;
    return RC_TIMEOUT;
  }
}

template<typename StreamerControl>
inline bool output_fifo_has_pending_data(StreamerControl &sc,
                                         uint64_t &ctr_in,
                                         uint64_t &ctr_out)
{
  ctr_in = sc.get_output_fifo_ctr_in();
  ctr_out = sc.get_output_fifo_ctr_out();
  return ctr_in > ctr_out;
}

template<typename StreamerControl>
inline int wait_for_output_fifo_data(StreamerControl &sc,
                                     const Verbosity &v,
                                     const uint64_t max_cnt = default_output_fifo_ready_timeout_ms)
{
  if (v.veryverbose)
    std::cout << "Waiting for streamer output FIFO data before force trigger" << std::endl;

  uint64_t ctr_in = 0;
  uint64_t ctr_out = 0;
  for (uint64_t cnt = 0; ; cnt++) {
    if (output_fifo_has_pending_data(sc, ctr_in, ctr_out))
      return RC_OK;
    if (cnt >= max_cnt)
      break;
    sleep_1ms();
  }

  if (v.verbose) {
    std::cout << red << "activate_trigger(): " << output_fifo_ready_timeout_text
              << " (ctr_in=" << with_underscores(ctr_in)
              << " ctr_out=" << with_underscores(ctr_out) << ")." << rst << std::endl;
    sc.status_report();
  }
  return RC_TIMEOUT;
}

template<typename StreamerControl>
inline int force_trigger_when_output_fifo_ready(StreamerControl &sc,
                                               const Verbosity &v,
                                               const uint64_t output_fifo_ready_timeout_ms = default_output_fifo_ready_timeout_ms)
{
  const int rc = wait_for_output_fifo_data(sc, v, output_fifo_ready_timeout_ms);
  if (rc != RC_OK)
    return rc;
  if (v.verbose)
    std::cout << cyan << " ---> Forcing trigger." << rst << std::endl;
  sc.trigger_force();
  sc.status_report();
  return RC_OK;
}

template<typename StreamerControl>
inline int activate_trigger(StreamerControl &sc,
                            const InputParser &input,
                            const bool force_trigger,
                            const Verbosity &v,
                            const uint64_t output_fifo_ready_timeout_ms = default_output_fifo_ready_timeout_ms)
{
  if (force_trigger) {
    if (input.exists("-delay")) {
      const double delay = parse_time(input, "-delay", "0");
      sleepd(delay);
    }
    return force_trigger_when_output_fifo_ready(sc, v, output_fifo_ready_timeout_ms);
  } else {
    if (v.verbose)
      std::cout << cyan << " --->  Arming trigger." << rst << std::endl;
    sc.trigger_enable();
    sc.status_report();
  }
  return RC_OK;
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
                                     const ReadbackTimeoutPolicy &timeout_policy,
                                     const value_t initial_value,
                                     int &rc)
{
  bool rb_failure = false;
  if (input.exists("-check")) {
    // Readback comparison is done against the effective output stream, not necessarily
    // against the original input representation. `convert(...)` lets callers normalize
    // features such as non-BITLOAD updates before the check.
    convert_readback_reference(convert, elements, initial_value);
    drop_count0(elements);
    if (v.veryverbose && input.exists("-dump-converted"))
      elements.dump(std::cout, "% ");
    if (v.veryverbose)
      rb.check_fill_status();
    const auto successful = rb.check(std::move(elements), timeout_policy);
    if (!successful) {
      rb_failure = true;
      rc |= RC_ERROR_CHECK;
      if (rb.last_operation_timed_out())
        rc |= RC_TIMEOUT;
    }
  }
  return rb_failure;
}

inline void run_readback_dump_phase(readback &rb,
                                    const InputParser &input,
                                    const Verbosity &v,
                                    const ReadbackTimeoutPolicy &timeout_policy)
{
  if (input.exists("-read")) {
    if (v.veryverbose)
      rb.check_fill_status();
    rb.read_all(timeout_policy);
  }
}

inline int run_post_execution_checks(streamer_control &sc,
                                    readback &rb,
                                    counter &ctr,
                                    const std::optional<value_t> final,
                                    const bool rb_failure,
                                    const bool force_trigger,
                                    const InputParser &input,
                                    const Verbosity &v,
                                    int rc)
{
  // The remaining checks are post-completion invariants for the whole streamer path.
  bool post_execution_check_failure = false;
  const int wait_rc = sc.wait_to_complete(v);
  rc |= wait_rc;
  if (wait_rc & RC_TIMEOUT) {
    std::cout << red << "Skipping post-completion checks because the streamer "
              << streamer_completion_timeout_text << "." << rst << std::endl;
    deactivate_trigger(sc, force_trigger, v);
    return rc;
  }
  sleep_1ms();
  value_t final_qout = sc.get_qout();
  const bool match = final && final_qout == *final;
  std::cout << "send_and_trig(): Final qout=" << hex8(final_qout) << " [" << dec13(final_qout) << "]";
  if (!final) {
    std::cout << " not checked (expected value not inferred)" << std::endl;
  } else if (match) {
    std::cout << " OK" << std::endl;
  } else {
    if (!(envVarExists("PP_IGNORE_QOUT_FINAL") || input.exists("-pp_ignore_qout_final"))) {
      std::cout << red << " Mismatch: expecting " << hex8(*final) << rst << std::endl;
      post_execution_check_failure = true;
      rc |= RC_ERROR_CHECK;
    }
  }
  sc.statistics();
  if (sc.get_input_fifo1_ctr_in() != sc.get_input_fifo1_ctr_out()) {
    std::cout << red << "Mismatch in the streamer input FIFO1 detected." << rst << std::endl;
    post_execution_check_failure = true;
    rc |= RC_ERROR_CHECK;
  }
  if (sc.get_input_fifo2_ctr_in() != sc.get_input_fifo2_ctr_out()) {
    std::cout << red << "Mismatch in the streamer input FIFO2 detected." << rst << std::endl;
    post_execution_check_failure = true;
    rc |= RC_ERROR_CHECK;
  }
  if (sc.get_output_fifo_ctr_in() != sc.get_output_fifo_ctr_out()) {
    std::cout << red << "Mismatch in the streamer output FIFO detected." << rst << std::endl;
    post_execution_check_failure = true;
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
  if (crcOK && rb_failure && !post_execution_check_failure)
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
  const value_t initial_value = sc.get_qout_streamer();
  auto [working_elements, final] = prepare_sequence_for_streaming(elements, input, initial_value);
  dump_sequence(working_elements, v);
  rc |= transmit_sequence_checked(tr, sc, working_elements, v);
  if (rc & RC_TIMEOUT)
    return rc;
  const auto timeout_policy = readback_timeout_policy(input);
  const int trigger_rc = activate_trigger(sc, input, force_trigger, v);
  rc |= trigger_rc;
  if (trigger_rc != RC_OK)
    return rc;

  bool rb_failure = run_readback_check_phase(rb, working_elements, input, v, convert, timeout_policy, initial_value, rc);
  run_readback_dump_phase(rb, input, v, timeout_policy);

  // -dont_wait deliberately skips post-execution cleanup; armed/forced trigger
  // state may remain active until reset, reconfiguration, or explicit deactivation.
  if (input.exists("-dont_wait"))
    return rc;

  return run_post_execution_checks(sc, rb, ctr, final, rb_failure, force_trigger, input, v, rc);
}

template<typename Transport>
inline int send_and_trig(Transport &tr, streamer_control &sc, readback &rb, counter &ctr,
                        Sequence &elements, const InputParser &input, const bool force_trigger, const Verbosity &v) {
  return send_and_trig(tr, sc, rb, ctr, elements, input, force_trigger, v, convert_for_readback_check);
}

constexpr bool force_trigger = true;
constexpr bool do_not_force_trigger = false;
