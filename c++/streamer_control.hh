// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Rok Zitko
//
// High-level host-side control interface for the PulsePins streamer.

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
#include "delay.hh"
#include "fifo.hh"
#include "dma.hh"
#include "misc.hh"
#include "verbosity.hh"
#include "config.h"
#include "definitions.hh"
#include "colors.hh"

// Streamer control: high-level interface for controlling a run-length decoding streamer
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
                    const std::uintptr_t base) :
     sc(dev.get_loc(base, IF_CTRL*4)),                  // control (w), status (r)
     qout(dev.get_loc(base, QOUT*4)),                   // (r)
     qout_streamer(dev.get_loc(base, QOUT_STREAMER*4)), // (r)
     ext_trig_in(dev.get_loc(base, EXT_TRIG_IN*4)),     // (r)
     ext_trig_ctrl(dev.get_loc(base, EXT_TRIG_CTRL*4)), // (r)
     qout_override(dev.get_loc(base, QOUT_OVERRIDE*4)), // (w)
     initial_value(dev.get_loc(base, INIT_VAL*4)),      // (w)
     lgating(dev.get_loc(base, GATING_W*4)),            // (r&w)
     overflow(dev.get_loc(base, FIFO_OVERFLOW*4)),      // (r)
     crc32(dev.get_loc(base, CRC32*4)),                 // (r)
     input_fifo1_ctr_in_l  (dev.get_loc(base, INPUT_FIFO1_CTR_IN_L*4)),
     input_fifo1_ctr_in_h  (dev.get_loc(base, INPUT_FIFO1_CTR_IN_H*4)),
     input_fifo1_ctr_out_l (dev.get_loc(base, INPUT_FIFO1_CTR_OUT_L*4)),
     input_fifo1_ctr_out_h (dev.get_loc(base, INPUT_FIFO1_CTR_OUT_H*4)),
     input_fifo2_ctr_in_l  (dev.get_loc(base, INPUT_FIFO2_CTR_IN_L*4)),
     input_fifo2_ctr_in_h  (dev.get_loc(base, INPUT_FIFO2_CTR_IN_H*4)),
     input_fifo2_ctr_out_l (dev.get_loc(base, INPUT_FIFO2_CTR_OUT_L*4)),
     input_fifo2_ctr_out_h (dev.get_loc(base, INPUT_FIFO2_CTR_OUT_H*4)),
     output_fifo_ctr_in_l  (dev.get_loc(base, OUTPUT_FIFO_CTR_IN_L*4)),
     output_fifo_ctr_in_h  (dev.get_loc(base, OUTPUT_FIFO_CTR_IN_H*4)),
     output_fifo_ctr_out_l (dev.get_loc(base, OUTPUT_FIFO_CTR_OUT_L*4)),
     output_fifo_ctr_out_h (dev.get_loc(base, OUTPUT_FIFO_CTR_OUT_H*4))
     {};

   port_t status() {
     return sc.read();
   }

   port_t get_control() {
     return control;
   }

   port_t get_overflow() {
     return overflow.read();
   }

   port_t get_crc32() {
     return crc32.read();
   }

   // Output signal on the device pins
   value_t get_qout() {
     if constexpr (std::is_same_v<value_t, uint32_t>) {
       return qout.read();
     } else {
       assert(false && "Not implemented");
     }
   }

   // Output signal from the streamer
   value_t get_qout_streamer() {
     if constexpr (std::is_same_v<value_t, uint32_t>) {
       return qout_streamer.read();
     } else {
       assert(false && "Not implemented");
     }
   }

   uint64_t get_input_fifo1_ctr_in() {
     return (uint64_t(input_fifo1_ctr_in_h.read()) << 32) + uint64_t(input_fifo1_ctr_in_l.read());
   }

   uint64_t get_input_fifo1_ctr_out() {
     return (uint64_t(input_fifo1_ctr_out_h.read()) << 32) + uint64_t(input_fifo1_ctr_out_l.read());
   }

   uint64_t get_input_fifo2_ctr_in() {
     return (uint64_t(input_fifo2_ctr_in_h.read()) << 32) + uint64_t(input_fifo2_ctr_in_l.read());
   }

   uint64_t get_input_fifo2_ctr_out() {
     return (uint64_t(input_fifo2_ctr_out_h.read()) << 32) + uint64_t(input_fifo2_ctr_out_l.read());
   }

   uint64_t get_output_fifo_ctr_in() {
     return (uint64_t(output_fifo_ctr_in_h.read()) << 32) + uint64_t(output_fifo_ctr_in_l.read());
   }

   uint64_t get_output_fifo_ctr_out() {
     return (uint64_t(output_fifo_ctr_out_h.read()) << 32) + uint64_t(output_fifo_ctr_out_l.read());
   }

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

   // Initial value is the state of the output bits before the streaming is started.
   void set_initial_value(const value_t iv) {
     if constexpr (std::is_same_v<value_t, uint32_t>) {
       initial_value.write(iv);
     } else {
       assert(false && "Not implemented");
     }
   }

   // qout_override is the state of the output bits if we force their values; see qout_select() below
   void set_qout_override(const value_t v) {
     if constexpr (std::is_same_v<value_t, uint32_t>) {
       qout_override.write(v);
     } else {
       assert(false && "Not implemented");
     }
   }

   // Returns true if there was an error (FIFO buffer underflow) encountered during streaming out.
   bool buffer_error() {
     return status() & BUFFER_ERROR;
   }

   // Returns true if streaming out completed successfully.
   bool done() {
     return status() & DONE;
   }

   auto wait_to_complete(const Verbosity &v, const uint64_t max_cnt = 10000) { // 10s maximum wait time by default
     int rc = RC_OK;
     if (v.veryverbose)
       std::cout << "Waiting for streamer to complete" << std::endl;
     uint64_t cnt = 0;
     while (!(done() || buffer_error()) && cnt < max_cnt) { sleep_1ms(); cnt++; }
     if (cnt == max_cnt && v.verbose) {
       std::cout << "wait_to_complete(): timeout exceeded while waiting for completion." << std::endl;
       rc = RC_TIMEOUT;
     }
     if (v.veryverbose)
       status_report();
     return rc;
   }

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

   // Full reset: sets all registers to zero, then calls reset_streamer()
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

   // Trigger enable (internal signal)
   void trigger_enable() {
     BITMASK_SET(control, TRIGGER_ENABLE_INT);
     sc.write(control);
   }

   // Trigger force (internal signal)
   void trigger_force() {
     BITMASK_SET(control, TRIGGER_FORCE_INT);
     sc.write(control);
   }

   // Trigger reset (internal signal)
   void trigger_reset() {
     BITMASK_SET(control, TRIGGER_RESET_INT);
     sc.write(control);
   }

   // true: qout_override
   // false: qout_streamer
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

   // Set qout_override value and switch the output immediately to that value.
   void qout_set(const value_t v) {
     set_qout_override(v);
     qout_select(true); // must come *after* set_qout_override()
   }

   // Enable gating: the streaming out from the output FIFO in gating modes depends
   // on an enable (gate) signal. There is a dedicated input port (gate_in) wired to a
   // physical pin on the FPGA board. Alternatively, one can gate using the trigger
   // signals under the control for the gate_mask masking pattern.
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

   // Control gating parameters from a string (for parsing command-line options)
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
