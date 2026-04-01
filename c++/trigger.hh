// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// High-level CLI-facing control of the trigger combiner.
//
// This wrapper translates parsed trigger options into `combiner_trig` configuration so the
// streamer trigger path can be rerouted, masked, and inverted without manipulating the raw
// combiner registers directly. Architectural overview lives in `docs/docs/streamer.md`,
// `docs/docs/clock_domain.md`, and `docs/docs/cpp.md`.

#pragma once

#include <iostream>
#include <bitset>

#include "fpga.hh"
#include "combiner.hh"
#include "parser.hh"
#include "options.hh"

class trigger {
private:
  const FPGA &fpga;

public:
  combiner_trig ct;

  trigger(const TriggerOptions &opts, const FPGA &_fpga) :
    fpga(_fpga),
    ct(fpga.dev_h2f, COMBINER_TRIG_BASE) { set(opts); }

  trigger(const InputParser &input, const FPGA &_fpga) :
    trigger(resolve_trigger_options(input), _fpga) {}

  // Apply trigger-routing options. The default policy is internal triggering.
  void set(const TriggerOptions &opts) {
    // Only explicitly requested fields are programmed, so callers can update one aspect of
    // the trigger path without overwriting the rest of the combiner configuration.
    if (fpga.v.veryverbose) std::cout << "## Setting up the trigger combiner." << std::endl;
    if (opts.mode == TriggerModeOption::internal) {
      if (fpga.v.veryverbose) std::cout << "Trigger: internal" << std::endl;
      ct.mode(trig_mode::INT);
    }
    if (opts.mode == TriggerModeOption::external) {
      if (fpga.v.veryverbose) std::cout << "Trigger: external" << std::endl;
      ct.mode(trig_mode::EXT);
    }
    if (opts.mode == TriggerModeOption::misc) {
      if (fpga.v.veryverbose) std::cout << "Trigger: misc (pushbuttons + 1PPS)" << std::endl;
      ct.mode(trig_mode::MISC);
    }
    if (opts.mode == TriggerModeOption::any) {
      if (fpga.v.veryverbose) std::cout << "Trigger: any of" << std::endl;
      ct.mode(trig_mode::OR);
    }
    if (opts.mode == TriggerModeOption::all) {
      if (fpga.v.veryverbose) std::cout << "Trigger: all of" << std::endl;
      ct.mode(trig_mode::AND);
    }
    if (opts.mode == TriggerModeOption::standard) {
      if (fpga.v.veryverbose) std::cout << "Trigger: standard (any of)" << std::endl;
      ct.invert_ext(~0); // invert all external signals (they are pulled up!)
      ct.mode(trig_mode::OR);
    }
    if (opts.invert_result) {
      auto v = *opts.invert_result;
      if (fpga.v.veryverbose) std::cout << "Trig result inverting: " << trig_parse(v) << std::endl;
      ct.invert_result(v);
    }
    if (opts.invert_int) {
      auto v = *opts.invert_int;
      if (fpga.v.veryverbose) std::cout << "Trig int inverting: " << trig_parse(v) << std::endl;
      ct.invert_int(v);
    }
    if (opts.invert_ext) {
      auto v = *opts.invert_ext;
      if (fpga.v.veryverbose) std::cout << "Trig ext inverting: " << trig_parse(v) << std::endl;
      ct.invert_ext(v);
    }
    if (opts.invert_misc) {
      auto v = *opts.invert_misc;
      if (fpga.v.veryverbose) std::cout << "Trig misc inverting: " << trig_parse(v) << std::endl;
      ct.invert_misc(v);
    }
    if (opts.mask_int) {
      auto v = *opts.mask_int;
      if (fpga.v.veryverbose) std::cout << "Trig int mask: " << trig_parse(v) << std::endl;
      ct.mask_int(v);
    }
    if (opts.mask_ext) {
      auto v = *opts.mask_ext;
      if (fpga.v.veryverbose) std::cout << "Trig ext mask: " << trig_parse(v) << std::endl;
      ct.mask_ext(v);
    }
    if (opts.mask_misc) {
      auto v = *opts.mask_misc;
      if (fpga.v.veryverbose) std::cout << "Trig misc mask: " << trig_parse(v) << std::endl;
      ct.mask_misc(v);
    }
  }
};
