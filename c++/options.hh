// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Structured option resolution helpers shared across host-side tools.
//
// These helpers convert raw CLI/environment inputs into small typed policy objects. The
// goal is to keep tool entry points readable and to make shared startup/runtime choices
// explicit, especially for clock selection, PLL programming, trigger routing, and
// readback/frequency-meter behavior.

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "config.h"
#include "tidbit.hh"
#include "parser.hh"
#include "misc.hh"

enum class StreamerClockSource {
  internal,
  external,
  raw_select,
};

struct ClockSelectionOptions {
  std::optional<StreamerClockSource> source;
  std::optional<uint32_t> raw_select;
};

struct PllOptions {
  std::string profile;
  std::optional<uint32_t> charge_pump;
  std::optional<uint32_t> bandwidth;
};

struct ReadbackOptions {
  uint32_t mode = 1;
};

enum class TriggerModeOption {
  internal,
  external,
  misc,
  any,
  all,
  standard,
};

struct TriggerOptions {
  std::optional<TriggerModeOption> mode;
  std::optional<uint32_t> invert_result;
  std::optional<uint32_t> invert_int;
  std::optional<uint32_t> invert_ext;
  std::optional<uint32_t> invert_misc;
  std::optional<uint32_t> mask_int;
  std::optional<uint32_t> mask_ext;
  std::optional<uint32_t> mask_misc;
};

struct StreamerOptions {
  bool stop_on_buffer_error = false;
  value_t initial_value = 0;
  bool report_initial_value = false;
};

struct FreqMeterOptions {
  std::optional<double> correction_factor;
};

inline bool resolve_reset_FPGA(const InputParser &input) {
  bool reset_FPGA = false; // default
  if (envVarExists("PP_RESET_FPGA"))
    reset_FPGA = true;
  if (input.exists("-reset_FPGA"))
    reset_FPGA = true;
  return reset_FPGA;
}

inline ClockSelectionOptions resolve_clock_selection_options(const InputParser &input) {
  ClockSelectionOptions opts;
  // Later checks intentionally override earlier ones so the raw `-clk` selector remains
  // the most explicit request when multiple clock-selection knobs are present.
  if (envVarExists("PP_INT_CLK") || input.exists("-int_clk"))
    opts.source = StreamerClockSource::internal;
  if (envVarExists("PP_EXT_CLK") || input.exists("-ext_clk"))
    opts.source = StreamerClockSource::external;
  if (envVarExists("PP_CLK") || input.exists("-clk")) {
    opts.source = StreamerClockSource::raw_select;
    opts.raw_select = parse_value(input, "-clk", get_env("PP_CLK"));
  }
  return opts;
}

inline PllOptions resolve_core_pll_options(const InputParser &input) {
  PllOptions opts;
  opts.profile = input.get_string("-core_pll", get_env("PP_CORE_PLL"));
  if (input.exists("-core_pll_charge_pump"))
    opts.charge_pump = input.get_uint32("-core_pll_charge_pump", 1);
  if (input.exists("-core_pll_bandwidth"))
    opts.bandwidth = input.get_uint32("-core_pll_bandwidth", 7);
  return opts;
}

inline PllOptions resolve_int_pll_options(const InputParser &input) {
  PllOptions opts;
  opts.profile = input.get_string("-int_pll", get_env("PP_INT_PLL"));
  if (input.exists("-int_pll_charge_pump"))
    opts.charge_pump = input.get_uint32("-int_pll_charge_pump", 1);
  if (input.exists("-int_pll_bandwidth"))
    opts.bandwidth = input.get_uint32("-int_pll_bandwidth", 7);
  return opts;
}

inline ReadbackOptions resolve_readback_options(const InputParser &input) {
  ReadbackOptions opts;
  const std::string rbmode = if_nonempty_or(get_env("PP_RBMODE"), "1");
  opts.mode = parse_uint32(input, "-rbmode", rbmode);
  return opts;
}

inline TriggerOptions resolve_trigger_options(const InputParser &input) {
  TriggerOptions opts;
  if (input.exists("-trig_int"))
    opts.mode = TriggerModeOption::internal;
  if (input.exists("-trig_ext"))
    opts.mode = TriggerModeOption::external;
  if (input.exists("-trig_misc"))
    opts.mode = TriggerModeOption::misc;
  if (input.exists("-trig_any"))
    opts.mode = TriggerModeOption::any;
  if (input.exists("-trig_all"))
    opts.mode = TriggerModeOption::all;
  if (input.exists("-trig_std"))
    opts.mode = TriggerModeOption::standard;

  if (input.exists("-invert_trig_result"))
    opts.invert_result = parse_uint32(input, "-invert_trig_result", "0");
  if (input.exists("-invert_int"))
    opts.invert_int = parse_uint32(input, "-invert_int", "0");
  if (input.exists("-invert_ext"))
    opts.invert_ext = parse_uint32(input, "-invert_ext", "0");
  if (input.exists("-invert_misc"))
    opts.invert_misc = parse_uint32(input, "-invert_misc", "0");
  if (input.exists("-mask_int"))
    opts.mask_int = parse_uint32(input, "-mask_int", "0");
  if (input.exists("-mask_ext"))
    opts.mask_ext = parse_uint32(input, "-mask_ext", "0");
  if (input.exists("-mask_misc"))
    opts.mask_misc = parse_uint32(input, "-mask_misc", "0");
  return opts;
}

inline StreamerOptions resolve_streamer_options(const InputParser &input,
                                                const std::string &initial_value_param = "-i") {
  StreamerOptions opts;
  // `report_initial_value` tracks whether the caller explicitly requested a non-default
  // initial output state so command implementations can decide whether it is worth
  // mentioning in user-facing reports.
  opts.stop_on_buffer_error = input.exists("-stop_on_buffer_error") || input.exists("-sobe");
  opts.initial_value = parse_value(input, initial_value_param, "0");
  opts.report_initial_value = opts.initial_value != 0;
  return opts;
}

inline FreqMeterOptions resolve_freq_meter_options(const InputParser &input) {
  FreqMeterOptions opts;
  constexpr auto cli_rescale = "-freq_rescale";
  constexpr auto env_rescale = "PP_FREQ_RESCALE";
  if (envVarExists(env_rescale) || input.exists(cli_rescale))
    opts.correction_factor = parse_double(input, cli_rescale, get_env(env_rescale));
  return opts;
}
