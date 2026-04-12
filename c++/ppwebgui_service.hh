// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <string>

#include "basic_multi_dma.hh"
#include "counter.hh"
#include "pio.hh"
#include "ppwebgui_config.hh"
#include "ppwebgui_types.hh"
#include "qout.hh"
#include "readback.hh"
#include "trigger.hh"

// Owns the full ppwebgui hardware object graph.
//
// Important lifetime rule:
// - keep this controller in one anchored instance for the whole server lifetime
// - do not copy it, move it, or re-own it from wrapper/helper objects
// - pass access around only via pointer/reference to that anchored instance
//
// We have seen board-only crashes when refactors changed the storage/ownership of the
// hardware-owning object graph even though the functional logic was otherwise unchanged.
class WebGuiController {
public:
  WebGuiController(FPGA &fpga_, const WebGuiRuntimeConfig &config_, const Verbosity &verbosity_);
  ~WebGuiController();

  WebGuiController(const WebGuiController &) = delete;
  WebGuiController &operator=(const WebGuiController &) = delete;
  WebGuiController(WebGuiController &&) = delete;
  WebGuiController &operator=(WebGuiController &&) = delete;

  StatusSnapshot get_status_copy();
  void apply_streamer_override(const StreamerOverrideState &state);
  void apply_combiner_config(const CombinerRequest &request);
  void apply_trigger_config(const TriggerConfigRequest &request);
  ResetResult reset_hardware();
  StreamResult stream_text_sequence(StreamLaunchRequest request);
  void set_last_error(const std::string &message);

private:
  void publish_action_locked(const std::string &last_action, const std::string &last_error);
  void publish_stream_result_locked(const std::string &last_action,
                                    const std::string &last_error,
                                    int last_stream_rc,
                                    const std::string &stream_message);
  void reset_hardware_locked();
  PortState read_port_state_locked(combiner_qout &combiner, int index, uint32_t cfg);
  void sync_qout_combiner_shadow_locked();
  void sync_trigger_combiner_shadow_locked();
  CombinerRequest read_combiner_config_locked();
  TriggerConfigState read_trigger_config_locked();
  StatusSnapshot read_status_locked();
  void apply_port_locked(combiner_qout &combiner, int port, const PortState &state);
  void apply_streamer_override_locked(const StreamerOverrideState &state);
  void apply_combiner_config_locked(const CombinerRequest &request);
  void apply_trigger_config_locked(const TriggerConfigState &state);

  FPGA &fpga;
  const WebGuiRuntimeConfig &config;
  const Verbosity &verbosity;
  qout qout_ctrl;
  streamer play_streamer;
  trigger trigger_ctrl;
  readback readback_path;
  counter counters;
  pio_in pio_aux;
  combiner_qout &comb;
  combiner_trig &trig_comb;
  CombinerRequest combiner_base_config;
  StreamerOverrideState streamer_override;
  StatusSnapshot snapshot;
};
