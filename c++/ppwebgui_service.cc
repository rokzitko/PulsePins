// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_service.hh"

#include <exception>
#include <sstream>
#include <utility>
#include <vector>

#include "address_map.hh"
#include "freq_meter.hh"
#include "parser.hh"
#include "pll_calc.hh"
#include "pll_rules.hh"
#include "ppworkflow.hh"
#include "startup.hh"

namespace {

std::string clock_source_display_from_options(const ClockSelectionOptions &opts) {
  if (!opts.source) {
    return "startup default";
  }

  switch (*opts.source) {
  case StreamerClockSource::internal:
    return "int_clk";
  case StreamerClockSource::external:
    return "ext_clk";
  case StreamerClockSource::raw_select:
    return "raw -clk " + std::to_string(opts.raw_select.value_or(0));
  }

  return "unknown";
}

bool clock_source_is_managed(const ClockSelectionOptions &opts) {
  return opts.source == StreamerClockSource::internal || opts.source == StreamerClockSource::external;
}

ClockPllState pll_state_from_options(const PllOptions &opts) {
  return {opts.profile, opts.charge_pump, opts.bandwidth};
}

ClockConfigState clock_config_state_from_options(const ClockSelectionOptions &clock_selection,
                                                 const PllOptions &core,
                                                 const PllOptions &internal) {
  ClockConfigState state;
  state.selection = clock_selection;
  state.source_display = clock_source_display_from_options(clock_selection);
  state.source_managed = clock_source_is_managed(clock_selection);
  state.core = pll_state_from_options(core);
  state.internal = pll_state_from_options(internal);
  return state;
}

ClockSelectionOptions clock_selection_options_from_request(const ClockConfigRequest &request) {
  ClockSelectionOptions opts;
  opts.source = request.source == ClockSourceSelection::EXT_CLK
    ? StreamerClockSource::external
    : StreamerClockSource::internal;
  opts.raw_select.reset();
  return opts;
}

PllOptions pll_options_from_state(const ClockPllState &state) {
  return {state.profile, state.charge_pump, state.bandwidth};
}

void validate_pll_profile_string(const std::string &profile, const char *label) {
  try {
    (void)pllcalc::resolve_profile(profile, applyReplacement(profile, pll_rules));
  }
  catch (const std::exception &) {
    throw std::runtime_error(std::string("Invalid ") + label + " profile: '" + profile + "'");
  }
}

void validate_requested_pll_profile_string(const std::string &profile, const char *label) {
  try {
    (void)pllcalc::resolve_profile(profile, applyReplacement(profile, pll_rules));
  }
  catch (const std::exception &) {
    throw WebGuiBadRequest(std::string("Invalid ") + label + " profile: '" + profile + "'");
  }
}

void validate_clock_config_state(const ClockConfigState &state) {
  if (state.selection.source == StreamerClockSource::raw_select && !state.selection.raw_select.has_value()) {
    throw std::runtime_error("Clock config is missing the raw -clk selector value");
  }
  validate_pll_profile_string(state.core.profile, "core_clk");
  validate_pll_profile_string(state.internal.profile, "int_clk");
}

void validate_requested_clock_config_state(const ClockConfigState &state) {
  validate_requested_pll_profile_string(state.core.profile, "core_clk");
  validate_requested_pll_profile_string(state.internal.profile, "int_clk");
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
  const auto mode = opts.mode.value_or(TriggerModeOption::internal);

  switch (mode) {
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
  case TriggerModeSelection::INT:
    state.mode = static_cast<uint32_t>(trig_mode::INT);
    break;
  case TriggerModeSelection::EXT:
    state.mode = static_cast<uint32_t>(trig_mode::EXT);
    break;
  case TriggerModeSelection::MISC:
    state.mode = static_cast<uint32_t>(trig_mode::MISC);
    break;
  case TriggerModeSelection::ANY:
    state.mode = static_cast<uint32_t>(trig_mode::OR);
    break;
  case TriggerModeSelection::ALL:
    state.mode = static_cast<uint32_t>(trig_mode::AND);
    break;
  }
  state.invert_result = request.invert_result;
  state.invert_int = request.invert_int;
  state.invert_ext = request.invert_ext;
  state.invert_misc = request.invert_misc;
  state.mask_int = request.mask_int;
  state.mask_ext = request.mask_ext;
  state.mask_misc = request.mask_misc;
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
  pio_aux(fpga.dev_lw, address_map::lw::pio_aux.base),
  comb(qout_ctrl.cq),
  trig_comb(trigger_ctrl.ct) {
  snapshot.poll_ms = config.poll_ms;
  snapshot.clocking.tracked = clock_config_state_from_options(config.clock_selection, config.core_pll, config.int_pll);
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

void WebGuiController::apply_clock_config(const ClockConfigRequest &request) {
  auto lock = fpga.acquire_lock();
  const auto previous_clocking = read_clock_config_locked();
  const auto previous_trigger = read_trigger_config_locked();
  const auto previous_combiner = read_combiner_config_locked();
  const auto previous_override = snapshot.streamer.override_state;
  const auto requested = clock_config_from_request_locked(request);
  validate_requested_clock_config_state(requested);
  try {
    reset_hardware_locked(requested, previous_trigger, previous_combiner, previous_override);
  } catch (const std::exception &e) {
    restore_known_good_state_locked(previous_clocking, previous_trigger, previous_combiner, previous_override, e);
    throw;
  } catch (...) {
    restore_known_good_state_locked(
      previous_clocking,
      previous_trigger,
      previous_combiner,
      previous_override,
      std::runtime_error("Unhandled non-standard exception while applying clock config"));
    throw;
  }
  publish_action_locked("applied clock config", "");
}

void WebGuiController::measure_clocks() {
  auto lock = fpga.acquire_lock();
  measure_clocks_locked(true);
  publish_action_locked("remeasured clocks", "");
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
  const auto previous_clocking = read_clock_config_locked();
  const auto previous_trigger = read_trigger_config_locked();
  const auto previous_combiner = read_combiner_config_locked();
  const auto previous_override = snapshot.streamer.override_state;

  try {
    reset_hardware_locked(previous_clocking, previous_trigger, previous_combiner, previous_override);
  } catch (const std::exception &e) {
    restore_known_good_state_locked(previous_clocking, previous_trigger, previous_combiner, previous_override, e);
    throw;
  } catch (...) {
    restore_known_good_state_locked(
      previous_clocking,
      previous_trigger,
      previous_combiner,
      previous_override,
      std::runtime_error("Unhandled non-standard exception while resetting hardware"));
    throw;
  }

  publish_action_locked("reset hardware", "");
  return {true, "Hardware reset completed and web settings restored"};
}

StreamResult WebGuiController::stream_text_sequence(StreamLaunchRequest request) {
  try {
    std::stringstream sequence_stream(request.sequence_text);
    auto [sequence, parsed_force_trigger] = parse_sequence_from_stream(sequence_stream);
    if (explicit_final_output(sequence)) {
      throw WebGuiBadRequest("Sequence already contains an explicit final output; browser playback supplies its own final output");
    }
    const bool force_trigger_request = request.force_trigger_override.value_or(parsed_force_trigger);

    InputParser request_input(std::vector<std::string>{});
    if (request.check_readback) {
      request_input.add("-check");
    }

    auto lock = fpga.acquire_lock();
    const auto previous_clocking = read_clock_config_locked();
    const auto previous_trigger = read_trigger_config_locked();
    const auto previous_combiner = read_combiner_config_locked();
    const auto previous_override = snapshot.streamer.override_state;
    try {
      reset_hardware_locked(previous_clocking, previous_trigger, previous_combiner, previous_override);
    } catch (const std::exception &e) {
      restore_known_good_state_locked(previous_clocking, previous_trigger, previous_combiner, previous_override, e);
      throw;
    } catch (...) {
      restore_known_good_state_locked(
        previous_clocking,
        previous_trigger,
        previous_combiner,
        previous_override,
        std::runtime_error("Unhandled non-standard exception while preparing the streamer"));
      throw;
    }
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

    const bool timed_out = (rc & RC_TIMEOUT) != 0;
    const bool readback_timeout = timed_out && (rc & RC_ERROR_CHECK) != 0;
    const auto error = timed_out
      ? (readback_timeout
          ? std::string("Streaming timed out waiting for readback data with rc=") + std::to_string(rc)
          : std::string("Streaming ") + streamer_completion_timeout_text + " with rc=" + std::to_string(rc))
      : std::string("Streaming failed with rc=") + std::to_string(rc);
    publish_stream_result_locked("stream failed", error, rc, error);
    return {false, rc, timed_out ? 504 : 500, error};
  } catch (const WebGuiBadRequest &e) {
    auto lock = fpga.acquire_lock();
    publish_stream_result_locked("stream failed", e.what(), RC_INVALID_ARG, e.what());
    throw;
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
  reset_hardware_locked(read_clock_config_locked(), read_trigger_config_locked(), read_combiner_config_locked(), snapshot.streamer.override_state);
}

void WebGuiController::reset_hardware_locked(const ClockConfigState &clocking_state) {
  reset_hardware_locked(clocking_state, read_trigger_config_locked(), read_combiner_config_locked(), snapshot.streamer.override_state);
}

void WebGuiController::reset_hardware_locked(const ClockConfigState &clocking_state,
                                             const TriggerConfigState &trigger_state,
                                             const CombinerRequest &combiner_state,
                                             const StreamerOverrideState &override_state)
{
	if (perform_FPGA_s2f_reset)
		fpga.rm.s2f_reset(verbosity.verbose);
  apply_clock_config_locked(clocking_state, true);

  play_streamer.set_initial_value_opts(config.streamer_options);
  fpga.output_enable(true);
  play_streamer.sc.reset();
  readback_path.reset();
  counters.reset_all();
  snapshot.streamer.qout_streamer = config.streamer_options.initial_value;

  apply_trigger_config_locked(trigger_state);
  apply_combiner_config_locked(combiner_state);
  apply_streamer_override_locked(override_state);
}

void WebGuiController::restore_known_good_state_locked(const ClockConfigState &clocking_state,
                                                       const TriggerConfigState &trigger_state,
                                                       const CombinerRequest &combiner_state,
                                                       const StreamerOverrideState &override_state,
                                                       const std::exception &cause) {
  try {
    reset_hardware_locked(clocking_state, trigger_state, combiner_state, override_state);
  } catch (const std::exception &restore_error) {
    throw std::runtime_error(
      std::string("Hardware apply failed: ") + cause.what() +
      ". Failed to restore the previous web-managed state: " + restore_error.what());
  } catch (...) {
    throw std::runtime_error(
      std::string("Hardware apply failed: ") + cause.what() +
      ". Failed to restore the previous web-managed state due to a non-standard exception.");
  }
}

void WebGuiController::measure_clocks_locked(const bool report) {
  pp_freq_meter meter(config.freq_meter_options, fpga, true, report || verbosity.veryverbose);
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

ClockConfigState WebGuiController::clock_config_from_request_locked(const ClockConfigRequest &request) {
  ClockConfigState state = snapshot.clocking.tracked;
  state.selection = clock_selection_options_from_request(request);
  state.source_display = request.source == ClockSourceSelection::EXT_CLK ? "ext_clk" : "int_clk";
  state.source_managed = true;
  state.core.profile = request.core_profile;
  state.internal.profile = request.int_profile;
  return state;
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
  validate_clock_config_state(state);
  fpga.set_clk(state.selection);
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
