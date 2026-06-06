// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Rok Zitko
//
// High-level host-side control interface for the PulsePins streamer.
//
// This wrapper is the software-visible lifecycle controller for one streamer instance. It
// owns the control/status register, output-state visibility, gating configuration, overflow
// and CRC status, and the FIFO traffic counters used by higher-level workflow checks.
//
// Architectural overview lives in `c++/README.md`, `docs/docs/cpp.md`, and
// `docs/docs/streamer.md`.

#pragma once

#include <cassert>
#include <bitset>
#include <cstdint> // integer types, uint32_t, etc.
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h> // usleep
#include <type_traits> // is_same_v

#include "tidbit.hh"
#include "address_map.hh"
#include "delay.hh"
#include "fifo.hh"
#include "dma.hh"
#include "misc.hh"
#include "verbosity.hh"
#include "config.h"
#include "definitions.hh"
#include "colors.hh"

inline constexpr uint64_t default_streamer_completion_timeout_ms = 10'000;
inline constexpr const char *streamer_completion_timeout_text =
  "timed out waiting for streamer completion (10 s internal limit)";

// Register-level host wrapper for one streamer interface instance.
class streamer_control
{
private:
  loc sc,             // status & control
    qout,             // current value at the output (r)
    qout_streamer,    // current value at the streamer output (r)
    ext_trig_in,      // external (input to core) trig in signals
    ext_trig_ctrl,    // external (input to core) trig ctrl signals
    qout_override,    // overriding value of qout (w)
    initial_value,    // initial value of the output (w)
    lgating,          // gating control & status (r&w)
    overflow,         // overflow flags (r)
    crc32,            // CRC (r)
    input_fifo1_ctr_in_l,
    input_fifo1_ctr_in_h,
    input_fifo1_ctr_out_l,
    input_fifo1_ctr_out_h,
    input_fifo2_ctr_in_l,
    input_fifo2_ctr_in_h,
    input_fifo2_ctr_out_l,
    input_fifo2_ctr_out_h,
    output_fifo_ctr_in_l,
    output_fifo_ctr_in_h,
    output_fifo_ctr_out_l,
    output_fifo_ctr_out_h;
  // control = {stop_on_buffer_error, qout_select, trigger_reset_int, reset_streamer, trigger_enable_int, trigger_force_int, stop}
  port_t control_initial_value = 0; // default control register value (restored after each reset)
  port_t control = control_initial_value;

public:
  streamer_control(const mm &dev,
                    const address_map::H2fRegion base,
                    std::string name = "streamer_control"s) :
    sc(dev, base.base, IF_CTRL*4, name + "/ctrl"),                           // control (w), status (r)
    qout(dev, base.base, QOUT*4, name + "/qout"),                            // (r)
    qout_streamer(dev, base.base, QOUT_STREAMER*4, name + "/qout_streamer"), // (r)
    ext_trig_in(dev, base.base, EXT_TRIG_IN*4, name + "/ext_trig_in"),       // (r)
    ext_trig_ctrl(dev, base.base, EXT_TRIG_CTRL*4, name + "/ext_trig_ctrl"), // (r)
    qout_override(dev, base.base, QOUT_OVERRIDE*4, name + "/qout_override"), // (w)
    initial_value(dev, base.base, INIT_VAL*4, name + "/init_val"),           // (w)
    lgating(dev, base.base, GATING_W*4, name + "/gating"),                   // (r&w)
    overflow(dev, base.base, FIFO_OVERFLOW*4, name + "/overflow"),           // (r)
    crc32(dev, base.base, CRC32*4, name + "/crc32"),                         // (r)
    input_fifo1_ctr_in_l  (dev, base.base, INPUT_FIFO1_CTR_IN_L*4,  name + "/in_fifo1_ctr_in_l"),
    input_fifo1_ctr_in_h  (dev, base.base, INPUT_FIFO1_CTR_IN_H*4,  name + "/in_fifo1_ctr_in_h"),
    input_fifo1_ctr_out_l (dev, base.base, INPUT_FIFO1_CTR_OUT_L*4, name + "/in_fifo1_ctr_out_l"),
    input_fifo1_ctr_out_h (dev, base.base, INPUT_FIFO1_CTR_OUT_H*4, name + "/in_fifo1_ctr_out_h"),
    input_fifo2_ctr_in_l  (dev, base.base, INPUT_FIFO2_CTR_IN_L*4,  name + "/in_fifo2_ctr_in_l"),
    input_fifo2_ctr_in_h  (dev, base.base, INPUT_FIFO2_CTR_IN_H*4,  name + "/in_fifo2_ctr_in_h"),
    input_fifo2_ctr_out_l (dev, base.base, INPUT_FIFO2_CTR_OUT_L*4, name + "/in_fifo2_ctr_out_l"),
    input_fifo2_ctr_out_h (dev, base.base, INPUT_FIFO2_CTR_OUT_H*4, name + "/in_fifo2_ctr_out_h"),
    output_fifo_ctr_in_l  (dev, base.base, OUTPUT_FIFO_CTR_IN_L*4,  name + "/out_fifo_ctr_in_l"),
    output_fifo_ctr_in_h  (dev, base.base, OUTPUT_FIFO_CTR_IN_H*4,  name + "/out_fifo_ctr_in_h"),
    output_fifo_ctr_out_l (dev, base.base, OUTPUT_FIFO_CTR_OUT_L*4, name + "/out_fifo_ctr_out_l"),
    output_fifo_ctr_out_h (dev, base.base, OUTPUT_FIFO_CTR_OUT_H*4, name + "/out_fifo_ctr_out_h")
    {};

  // Read the combined status/control register as exposed by `st_interface.sv`.
  port_t status() {
    return sc.read();
  }

  // Return the software-side shadow of the control word.
  port_t get_control() {
    return control;
  }

  port_t get_overflow() {
    return overflow.read();
  }

  port_t get_crc32() {
    return crc32.read();
  }

  // Current value visible on the actual output pins after any qout override muxing.
  value_t get_qout() {
    if constexpr (std::is_same<value_t, uint32_t>::value) {
      return qout.read();
    }
    static_assert(std::is_same<value_t, uint32_t>::value, "Only uint32_t value_t is currently supported");
  }

  // Raw streamer output value before the optional qout override path.
  value_t get_qout_streamer() {
    if constexpr (std::is_same<value_t, uint32_t>::value) {
      return qout_streamer.read();
    }
    static_assert(std::is_same<value_t, uint32_t>::value, "Only uint32_t value_t is currently supported");
  }

  trigger_t get_ext_trig_in() {
    return static_cast<trigger_t>(ext_trig_in.read() & TRIGGER_MASK);
  }

  port_t get_ext_trig_ctrl() {
    return ext_trig_ctrl.read();
  }

  uint64_t get_input_fifo1_ctr_in() {
    return read_stable_u64([this] { return input_fifo1_ctr_in_l.read(); },
                           [this] { return input_fifo1_ctr_in_h.read(); });
  }

  uint64_t get_input_fifo1_ctr_out() {
    return read_stable_u64([this] { return input_fifo1_ctr_out_l.read(); },
                           [this] { return input_fifo1_ctr_out_h.read(); });
  }

  uint64_t get_input_fifo2_ctr_in() {
    return read_stable_u64([this] { return input_fifo2_ctr_in_l.read(); },
                           [this] { return input_fifo2_ctr_in_h.read(); });
  }

  uint64_t get_input_fifo2_ctr_out() {
    return read_stable_u64([this] { return input_fifo2_ctr_out_l.read(); },
                           [this] { return input_fifo2_ctr_out_h.read(); });
  }

  uint64_t get_output_fifo_ctr_in() {
    return read_stable_u64([this] { return output_fifo_ctr_in_l.read(); },
                           [this] { return output_fifo_ctr_in_h.read(); });
  }

  uint64_t get_output_fifo_ctr_out() {
    return read_stable_u64([this] { return output_fifo_ctr_out_l.read(); },
                           [this] { return output_fifo_ctr_out_h.read(); });
  }

  // Print transport/fifo accounting information used by higher-level verification paths.
  void statistics() {
    const auto i_ctr1_in  = get_input_fifo1_ctr_in();
    const auto i_ctr1_out = get_input_fifo1_ctr_out();
    std::cout << "input_fifo1: ctr_in=" << with_underscores(i_ctr1_in)
      << " ctr_out=" << with_underscores(i_ctr1_out)
      << " " << (i_ctr1_in == i_ctr1_out ? "OK" : "MISMATCH") << std::endl;
    const auto i_ctr2_in  = get_input_fifo2_ctr_in();
    const auto i_ctr2_out = get_input_fifo2_ctr_out();
    std::cout << "input_fifo2: ctr_in=" << with_underscores(i_ctr2_in)
      << " ctr_out=" << with_underscores(i_ctr2_out)
      << " " << (i_ctr2_in == i_ctr2_out ? "OK" : "MISMATCH") << std::endl;
    const auto o_ctr_in  = get_output_fifo_ctr_in();
    const auto o_ctr_out = get_output_fifo_ctr_out();
    std::cout << "output_fifo: ctr_in=" << with_underscores(o_ctr_in)
      << " ctr_out=" << with_underscores(o_ctr_out)
        << " " << (o_ctr_in == o_ctr_out ? "OK" : "MISMATCH") << std::endl;
    const auto over = get_overflow();
    if (over)
      std::cout << "input FIFO overflow: " << std::bitset<2>(over)
        << (over & 1 ? " {input} " : "")
        << (over & 2 ? " {output} " : "") << std::endl;
  }

  // Report the status of external trigger: for testing/debugging purposes
  // Note: external trigger is here meant external with respect to the streamer core, i.e., the external
  // trigger signals are those that are received from outside the streamer module through the st_interface input ports.
  void monitor_ext_trig() {
    for (uint64_t i = 0 ; ; i++) {
      const trigger_t in = ext_trig_in.read();
      const port_t ctrl = ext_trig_ctrl.read();
      std::cout << std::dec << i<< ": trig in=" << std::bitset<WIDTH_TRIGGER>(in)
        << (ctrl & EXT_TRIG_CTRL_ENABLE ? " [enable]" : "")
        << (ctrl & EXT_TRIG_CTRL_FORCE  ? " [force]"  : "")
        << (ctrl & EXT_TRIG_CTRL_RESET  ? " [reset]"  : "") << std::endl;
      usleep(100*1000); // reasonable delay for "real-time" monitoring on console
    }
  }

  // State of the output bits before the streamer has triggered and begun advancing.
  void set_initial_value(const value_t iv) {
    if constexpr (std::is_same<value_t, uint32_t>::value) {
      initial_value.write(iv);
    }
    static_assert(std::is_same<value_t, uint32_t>::value, "Only uint32_t value_t is currently supported");
  }

  // Value presented on the outputs when the qout override path is selected.
  void set_qout_override(const value_t v) {
    if constexpr (std::is_same<value_t, uint32_t>::value) {
      qout_override.write(v);
    }
    static_assert(std::is_same<value_t, uint32_t>::value, "Only uint32_t value_t is currently supported");
  }

  // True if the output side encountered an underrun/buffer error during streaming.
  bool buffer_error() {
    return status() & BUFFER_ERROR;
  }

  // True if the streamer reached the end of the buffered output stream.
  bool done() {
    return status() & DONE;
  }

  auto wait_to_complete(const Verbosity &v, const uint64_t max_cnt = default_streamer_completion_timeout_ms) {
    int rc = RC_OK;
    if (v.veryverbose)
      std::cout << "Waiting for streamer to complete" << std::endl;
    uint64_t cnt = 0;
    while (!(done() || buffer_error()) && cnt < max_cnt) { sleep_1ms(); cnt++; }
    if (cnt == max_cnt) {
      rc = RC_TIMEOUT;
      if (v.verbose)
        std::cout << "wait_to_complete(): " << streamer_completion_timeout_text << "." << std::endl;
    }
    if (v.veryverbose)
      status_report();
    return rc;
  }

  // Human-readable state dump combining status bits and the current software control shadow.
  void status_report(std::ostream &F = std::cout) {
    // Status bits
    const auto st = status();
    F << "Streamer status: (";
    if (st & BUFFER_ERROR)
      F << red << "[buffer error] " << rst;
    if (st & DONE)
      F << green << "[done] " << rst;
    if (st & TRIGGERED)
      F << cyan << "[triggered] " << rst;
    if (st & ARMED)
      F << yellow << "[armed] " << rst;
    // Control bits
    const auto co = get_control();
    if (co & STOP)
      F << red << "{stop} " << rst;
    if (co & TRIGGER_FORCE_INT)
      F << cyan << "{trigger_force_int} " << rst;
    if (co & TRIGGER_ENABLE_INT)
      F << yellow << "{trigger_enable_int} " << rst;
    if (co & RESET)
      F << red << "{reset} " << rst;
    if (co & TRIGGER_RESET_INT)
      F << red << "{trigger_reset_int} " << rst;
    if (co & QOUT_SELECT)
      F << magenta << "{qout_select} " << rst;
    if (co & STOP_ON_BUFFER_ERROR)
      F << blue << "{stop_on_buffer_error} " << rst;
    F << ")" << std::endl;
  }

  // Asserts reset signal for 'usleep_time' microseconds
  void reset_streamer(const uint32_t usleep_time = 10) {
    BITMASK_SET(control, RESET);
    sc.write(control);
    usleep(usleep_time);
    BITMASK_CLEAR(control, RESET);
    sc.write(control);
  }

  // Full software-visible reset: restore the control word to its persistent defaults, then
  // pulse the streamer reset bit.
  void reset(const uint32_t usleep_time = 10) {
    control = control_initial_value;
    sc.write(control);
    reset_streamer(usleep_time);
  }

  // Unconditionally stop streaming
  void stop(bool s) {
    if (s)  BITMASK_SET(control, STOP);
    if (!s) BITMASK_CLEAR(control, STOP);
    sc.write(control);
  }

  // Arm the internal trigger path; actual triggering still depends on trigger conditions.
  void trigger_enable() {
    BITMASK_SET(control, TRIGGER_ENABLE_INT);
    sc.write(control);
  }

  void trigger_disable() {
    BITMASK_CLEAR(control, TRIGGER_ENABLE_INT);
    sc.write(control);
  }

  // Force the internal trigger path active immediately.
  void trigger_force() {
    BITMASK_SET(control, TRIGGER_FORCE_INT);
    sc.write(control);
  }

  void trigger_clear() {
    BITMASK_CLEAR(control, TRIGGER_FORCE_INT);
    sc.write(control);
  }

  // Reset the internal trigger state machine/path.
  void trigger_reset() {
    BITMASK_SET(control, TRIGGER_RESET_INT);
    sc.write(control);
  }

  // Select between the override value and the raw streamer output.
  void qout_select(bool s) {
    if (s)  BITMASK_SET(control, QOUT_SELECT);
    if (!s) BITMASK_CLEAR(control, QOUT_SELECT);
    sc.write(control);
  }

  // If buffer_error goes high, trigger_activated is deasserted (and streaming stops).
  // This function also modifies control_initial_value, so the setting is persistent across
  // streamer resets through the reset() member function.
  void stop_on_buffer_error(bool s) {
    if (s) {
      BITMASK_SET(control, STOP_ON_BUFFER_ERROR);
      BITMASK_SET(control_initial_value, STOP_ON_BUFFER_ERROR);
    }
    if (!s) {
      BITMASK_CLEAR(control, STOP_ON_BUFFER_ERROR);
      BITMASK_CLEAR(control_initial_value, STOP_ON_BUFFER_ERROR);
    }
    sc.write(control);
  }

  // Convenience helper: update the override register and make it visible immediately.
  void qout_set(const value_t v) {
    set_qout_override(v);
    qout_select(true); // must come *after* set_qout_override()
  }

  // Configure output gating. The effective gate can come from the dedicated gate input,
  // from a masked subset of trigger inputs, or from both.
  void gating(const bool en, const bool gate_in_en, const trigger_t gate_mask) {
    port_t x = (en ? 1 : 0) + (gate_in_en ? 2 : 0) + (port_t(gate_mask) << 2);
    lgating.write(x);
  }

  std::string gate_status_string_from_x(const port_t x) {
    const bool en             = x & (1UL << 0);
    const bool gate_in_en     = x & (1UL << 1);
    const trigger_t gate_mask = (x >> 2) & TRIGGER_MASK;
    const bool gate_in        = x & (1UL << (1+WIDTH_TRIGGER+1));
    const bool gate_signal    = x & (1UL << (1+WIDTH_TRIGGER+2));
    const bool gate_enable    = x & (1UL << (1+WIDTH_TRIGGER+3));
    std::stringstream s;
    s << (en ? "[enabled] " : "") << (gate_in_en ? "[gate in enabled] " : "") << "gate_mask=" << std::bitset<WIDTH_TRIGGER>(gate_mask)
      << " " << (gate_in ? "{in} " : "") << (gate_signal ? "{signal} " : "") << (gate_enable ? "{enable}" : "");
    return s.str();
  }

  auto gate_status() {
    return lgating.read();
  }

  std::string gate_status_string() {
    return gate_status_string_from_x(gate_status());
  }

  // Parse the compact CLI gating syntax `enabled[:gate_in[:mask]]`.
  void set_gating_from_string(std::string s) {
    bool en = false;
    bool gate_in_en = false;
    trigger_t gate_mask = 0;
    if (s != "") {
      auto [a, b, c] = split_twice(s, ':');
      en = parse_bool(a);
      if (en && b != std::string_view{})
        gate_in_en = parse_bool(b);
      if (en && c != std::string_view{})
        gate_mask = parse_trigger_t(std::string(c));
    }
    gating(en, gate_in_en, gate_mask);
  }
};
