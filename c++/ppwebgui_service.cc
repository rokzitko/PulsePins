// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_service.hh"

#include <sstream>
#include <utility>
#include <vector>

#include "freq_meter.hh"
#include "parser.hh"
#include "ppworkflow.hh"
#include "startup.hh"

namespace {

ClockSourceSelection clock_source_from_options(const ClockSelectionOptions &opts) {
  if (opts.source == StreamerClockSource::external) {
    return ClockSourceSelection::EXT_CLK;
  }
  if (opts.source == StreamerClockSource::raw_select && opts.raw_select && *opts.raw_select == ch_ext) {
    return ClockSourceSelection::EXT_CLK;
  }
  return ClockSourceSelection::INT_CLK;
}

ClockPllState pll_state_from_options(const PllOptions &opts) {
  return {opts.profile, opts.charge_pump, opts.bandwidth};
}

ClockSelectionOptions clock_selection_options_from_state(const ClockConfigState &state) {
  ClockSelectionOptions opts;
  opts.source = state.source == ClockSourceSelection::EXT_CLK
    ? StreamerClockSource::external
    : StreamerClockSource::internal;
  return opts;
}

PllOptions pll_options_from_state(const ClockPllState &state) {
  return {state.profile, state.charge_pump, state.bandwidth};
}

uint32_t pack_live_trigger_status(const trigger_t trigger_in, const port_t trigger_ctrl) {
  uint32_t value = trigger_in & TRIGGER_MASK;
  if (trigger_ctrl & EXT_TRIG_CTRL_ENABLE) value |= (1U << PIO1_ENABLE);
  if (trigger_ctrl & EXT_TRIG_CTRL_FORCE) value |= (1U << PIO1_FORCE);
  if (trigger_ctrl & EXT_TRIG_CTRL_RESET) value |= (1U << PIO1_RESET);
  return value;
}

TriggerConfigState trigger_config_from_options(const TriggerOptions &opts) {
  TriggerConfigState state;
  state.mode = 0;

  if (opts.mode) {
    switch (*opts.mode) {
    case TriggerModeOption::internal:
      state.mode = static_cast<uint32_t>(trig_mode::INT);
      break;
    case TriggerModeOption::external:
      state.mode = static_cast<uint32_t>(trig_mode::EXT);
      break;
    case TriggerModeOption::misc:
      state.mode = static_cast<uint32_t>(trig_mode::MISC);
      break;
    case TriggerModeOption::any:
      state.mode = static_cast<uint32_t>(trig_mode::OR);
      break;
    case TriggerModeOption::all:
      state.mode = static_cast<uint32_t>(trig_mode::AND);
      break;
    case TriggerModeOption::standard:
      state.mode = static_cast<uint32_t>(trig_mode::OR);
      state.invert_ext = ~uint32_t {0};
      break;
    }
  }

  if (opts.invert_result) state.invert_result = *opts.invert_result;
  if (opts.invert_int) state.invert_int = *opts.invert_int;
  if (opts.invert_ext) state.invert_ext = *opts.invert_ext;
  if (opts.invert_misc) state.invert_misc = *opts.invert_misc;
  if (opts.mask_int) state.mask_int = *opts.mask_int;
  if (opts.mask_ext) state.mask_ext = *opts.mask_ext;
  if (opts.mask_misc) state.mask_misc = *opts.mask_misc;
  return state;
}

TriggerConfigState trigger_config_from_request(const TriggerConfigRequest &request,
                                               const TriggerConfigState &current) {
  TriggerConfigState state = current;
  switch (request.mode) {
  case TriggerModeSelection::STANDARD:
    state.mode = static_cast<uint32_t>(trig_mode::OR);
    state.invert_ext = ~uint32_t {0};
    break;
  case TriggerModeSelection::INT:
    state.mode = static_cast<uint32_t>(trig_mode::INT);
    state.invert_ext = request.invert_ext;
    break;
  case TriggerModeSelection::EXT:
    state.mode = static_cast<uint32_t>(trig_mode::EXT);
    state.invert_ext = request.invert_ext;
    break;
  case TriggerModeSelection::MISC:
    state.mode = static_cast<uint32_t>(trig_mode::MISC);
    state.invert_ext = request.invert_ext;
    break;
  case TriggerModeSelection::ANY:
    state.mode = static_cast<uint32_t>(trig_mode::OR);
    state.invert_ext = request.invert_ext;
    break;
  case TriggerModeSelection::ALL:
    state.mode = static_cast<uint32_t>(trig_mode::AND);
    state.invert_ext = request.invert_ext;
    break;
  }
  state.invert_result = request.invert_result;
  state.invert_int = request.invert_int;
  state.invert_misc = request.invert_misc;
  state.mask_int = request.mask_int;
  state.mask_ext = request.mask_ext;
  state.mask_misc = request.mask_misc;
  if (request.mode != TriggerModeSelection::STANDARD) {
    state.invert_ext = request.invert_ext;
  }
  return state;
}

bool force_enabled(const uint32_t cfg, const int port) {
  switch (port) {
  case 0: return cfg & (1U << B_FORCEo);
  case 1: return cfg & (1U << B_FORCE1);
  case 2: return cfg & (1U << B_FORCE2);
  case 3: return cfg & (1U << B_FORCE3);
  case 4: return cfg & (1U << B_FORCE4);
  default: throw std::runtime_error("Unexpected combiner port index");
  }
}

} // namespace

WebGuiController::WebGuiController(FPGA &fpga_, const WebGuiRuntimeConfig &config_, const Verbosity &verbosity_) :
  fpga(fpga_),
  config(config_),
  verbosity(verbosity_),
  qout_ctrl(verbosity, fpga),
  play_streamer(config.streamer_options, fpga),
  trigger_ctrl(config.trigger_options, fpga),
  readback_path(config.readback_options, fpga),
  counters(fpga),
  pio_aux(fpga.dev_lw, PIO_AUX_BASE),
  comb(qout_ctrl.cq),
  trig_comb(trigger_ctrl.ct) {
  snapshot.poll_ms = config.poll_ms;
  snapshot.clocking.tracked.source = clock_source_from_options(config.clock_selection);
  snapshot.clocking.tracked.core = pll_state_from_options(config.core_pll);
  snapshot.clocking.tracked.internal = pll_state_from_options(config.int_pll);
  snapshot.trigger_settings = trigger_config_from_options(config.trigger_options);
  combiner_base_config = config.combiner_request;
  snapshot.combiner_mode = to_string(combiner_base_config.mode);
  snapshot.output = combiner_base_config.output;
  snapshot.inputs = combiner_base_config.inputs;
  snapshot.streamer.qout_streamer = config.streamer_options.initial_value;
  streamer_override.enabled = (play_streamer.sc.get_control() & QOUT_SELECT) == QOUT_SELECT;
  snapshot.streamer.override_state = streamer_override;
  snapshot.streamer.qout = streamer_override.enabled ? streamer_override.value : snapshot.streamer.qout_streamer;
  apply_combiner_config_locked(combiner_base_config);
  measure_clocks_locked(false);
}

WebGuiController::~WebGuiController() = default;

StatusSnapshot WebGuiController::get_status_copy() {
  auto lock = fpga.acquire_lock();
  return read_status_locked();
}

void WebGuiController::apply_streamer_override(const StreamerOverrideState &state) {
  auto lock = fpga.acquire_lock();
  apply_streamer_override_locked(state);
  publish_action_locked("applied streamer override", "");
}

void WebGuiController::apply_combiner_config(const CombinerRequest &request) {
  auto lock = fpga.acquire_lock();
  apply_combiner_config_locked(request);
  publish_action_locked("applied combiner config", "");
}

void WebGuiController::apply_trigger_config(const TriggerConfigRequest &request) {
  auto lock = fpga.acquire_lock();
  apply_trigger_config_locked(trigger_config_from_request(request, read_trigger_config_locked()));
  publish_action_locked("applied trigger config", "");
}

ResetResult WebGuiController::reset_hardware() {
  auto lock = fpga.acquire_lock();
  reset_hardware_locked();

  publish_action_locked("reset hardware", "");
  return {true, "Hardware reset completed and web settings restored"};
}

StreamResult WebGuiController::stream_text_sequence(StreamLaunchRequest request) {
  try {
    std::stringstream sequence_stream(request.sequence_text);
    auto [sequence, parsed_force_trigger] = parse_sequence_from_stream(sequence_stream);
    const bool force_trigger_request = request.force_trigger_override.value_or(parsed_force_trigger);

    InputParser request_input(std::vector<std::string>{});
    if (request.check_readback) {
      request_input.add("-check");
    }

    auto lock = fpga.acquire_lock();
    reset_hardware_locked();
    const value_t final_value = snapshot.streamer.qout_streamer;
    request_input.add_with_arg("-t", hex8(final_value));
    const int rc = send_and_trig(
      play_streamer.fifo,
      play_streamer.sc,
      readback_path,
      counters,
      sequence,
      request_input,
      force_trigger_request,
      verbosity);
    if (rc == RC_OK) {
      snapshot.streamer.qout_streamer = final_value;
      if (!snapshot.streamer.override_state.enabled) {
        snapshot.streamer.qout = final_value;
      }
      const auto message = std::string("Sequence streamed successfully; final qout ") + hex8(final_value);
      publish_stream_result_locked("streamed sequence", "", rc, message);
      return {true, rc, 200, message};
    }

    const auto error = std::string("Streaming failed with rc=") + std::to_string(rc);
    publish_stream_result_locked("stream failed", error, rc, error);
    return {false, rc, 500, error};
  } catch (const std::exception &e) {
    auto lock = fpga.acquire_lock();
    publish_stream_result_locked("stream failed", e.what(), RC_EXCEPTION, e.what());
    throw;
  } catch (...) {
    auto lock = fpga.acquire_lock();
    publish_stream_result_locked(
      "stream failed",
      "Unhandled non-standard exception in stream worker",
      RC_EXCEPTION,
      "Unhandled non-standard exception in stream worker");
    throw;
  }
}

void WebGuiController::set_last_error(const std::string &message) {
  auto lock = fpga.acquire_lock();
  snapshot.last_error = message;
  snapshot.seqno++;
}

void WebGuiController::publish_action_locked(const std::string &last_action,
                                             const std::string &last_error) {
  snapshot.seqno += 1;
  snapshot.last_action = last_action;
  snapshot.last_error = last_error;
}

void WebGuiController::publish_stream_result_locked(const std::string &last_action,
                                                    const std::string &last_error,
                                                    const int last_stream_rc,
                                                    const std::string &stream_message) {
  publish_action_locked(last_action, last_error);
  snapshot.last_stream_rc = last_stream_rc;
  snapshot.stream_message = stream_message;
}

void WebGuiController::reset_hardware_locked() {
  const auto preserved_combiner = read_combiner_config_locked();
  const auto preserved_clocking = read_clock_config_locked();
  const auto preserved_trigger = read_trigger_config_locked();
  const auto preserved_override = snapshot.streamer.override_state;

  rstmgr rm;
  rm.s2f_reset(verbosity.verbose);
  apply_clock_config_locked(preserved_clocking, true);

  play_streamer.set_initial_value_opts(config.streamer_options);
  fpga.output_enable(true);
  play_streamer.sc.reset();
  readback_path.reset();
  counters.reset_all();
  snapshot.streamer.qout_streamer = config.streamer_options.initial_value;

  apply_trigger_config_locked(preserved_trigger);
  apply_combiner_config_locked(preserved_combiner);
  apply_streamer_override_locked(preserved_override);
}

void WebGuiController::measure_clocks_locked(const bool report) {
  pp_freq_meter meter(config.freq_meter_options, fpga, true, report || verbosity.verbose);
  snapshot.clocking.measured.ext_clk_hz = meter.meter.read_freq(METER_EXT_CLK);
  snapshot.clocking.measured.int_clk_hz = meter.meter.read_freq(METER_INT_CLK);
  snapshot.clocking.measured.streamer_clk_hz = meter.meter.read_freq(METER_STREAMER_CLK);
  snapshot.clocking.measured.core_clk_hz = meter.meter.read_freq(METER_CORE_CLK);
  if (report) {
    meter.report();
  }
}

PortState WebGuiController::read_port_state_locked(combiner_qout &combiner, const int index, const uint32_t cfg) {
  PortState state;
  state.invert = combiner.get_invert(index);
  state.mask = combiner.get_mask(index);
  state.force_enabled = force_enabled(cfg, index);
  state.force_value = combiner.get_force(index);
  return state;
}

void WebGuiController::sync_qout_combiner_shadow_locked() {
  const auto cfg = comb.get_cfg();
  comb.cfg(cfg);
}

void WebGuiController::sync_trigger_combiner_shadow_locked() {
  const auto cfg = trig_comb.get_cfg();
  trig_comb.cfg(cfg);
}

CombinerRequest WebGuiController::read_combiner_config_locked() {
  return combiner_base_config;
}

TriggerConfigState WebGuiController::read_trigger_config_locked() {
  return snapshot.trigger_settings;
}

ClockConfigState WebGuiController::read_clock_config_locked() {
  return snapshot.clocking.tracked;
}

StatusSnapshot WebGuiController::read_status_locked() {
  StatusSnapshot status = snapshot;
  status.poll_ms = config.poll_ms;
  status.aux_raw = static_cast<uint8_t>(pio_aux.read() & 0xffU);
  status.streamer.status_raw = play_streamer.sc.status();
  status.trig_raw = pack_live_trigger_status(
    play_streamer.sc.get_ext_trig_in(),
    play_streamer.sc.get_ext_trig_ctrl());
  return status;
}

void WebGuiController::apply_clock_config_locked(const ClockConfigState &state, const bool report_measurements) {
  fpga.set_clk(clock_selection_options_from_state(state));
  fpga.pll_core.set_core_clk(pll_options_from_state(state.core), verbosity);
  fpga.pll_int.set_int_clk(pll_options_from_state(state.internal), verbosity);
  snapshot.clocking.tracked = state;
  measure_clocks_locked(report_measurements);
}

void WebGuiController::apply_port_locked(combiner_qout &combiner, const int port, const PortState &state) {
  combiner.invert(port, state.invert);
  combiner.mask(port, state.mask);
  if (state.force_enabled) {
    combiner.force(port, state.force_value);
  } else {
    combiner.value(port, state.force_value);
    combiner.release_force(port);
  }
}

void WebGuiController::apply_streamer_override_locked(const StreamerOverrideState &state) {
  // The dedicated qout-override register faults on the deployed bitstream, so implement the
  // web override with the output combiner's final-output force path instead.
  play_streamer.sc.qout_select(false);
  streamer_override = state;
  snapshot.streamer.override_state = state;
  if (state.enabled) {
    snapshot.streamer.qout = state.value;
  } else {
    snapshot.streamer.qout = snapshot.streamer.qout_streamer;
  }
  apply_combiner_config_locked(combiner_base_config);
}

void WebGuiController::apply_combiner_config_locked(const CombinerRequest &request) {
  sync_qout_combiner_shadow_locked();
  combiner_base_config = request;

  CombinerRequest effective = request;
  if (streamer_override.enabled) {
    effective.output.force_enabled = true;
    effective.output.force_value = streamer_override.value;
  }

  comb.mode(effective.mode);
  apply_port_locked(comb, 0, effective.output);
  for (size_t i = 0; i < effective.inputs.size(); ++i) {
    apply_port_locked(comb, static_cast<int>(i + 1), effective.inputs[i]);
  }
  snapshot.combiner_mode = to_string(request.mode);
  snapshot.output = request.output;
  snapshot.inputs = request.inputs;
}

void WebGuiController::apply_trigger_config_locked(const TriggerConfigState &state) {
  sync_trigger_combiner_shadow_locked();
  trig_comb.mode(static_cast<trig_mode>(state.mode & MODE_MASK));
  trig_comb.invert_result(state.invert_result);
  trig_comb.invert_int(state.invert_int);
  trig_comb.invert_ext(state.invert_ext);
  trig_comb.invert_misc(state.invert_misc);
  trig_comb.invert(4, state.invert_aux);
  trig_comb.mask_int(state.mask_int);
  trig_comb.mask_ext(state.mask_ext);
  trig_comb.mask_misc(state.mask_misc);
  trig_comb.mask(4, state.mask_aux);
  snapshot.trigger_settings = state;
}
