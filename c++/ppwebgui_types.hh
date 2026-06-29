// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

#include "combiner.hh"
#include "definitions.hh"
#include "options.hh"

struct PortState {
  uint32_t invert = 0;
  uint32_t mask = 0;
  bool force_enabled = false;
  uint32_t force_value = 0;
};

struct WebGuiBadRequest : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct StreamerOverrideState {
  bool enabled = false;
  uint32_t value = 0;
};

struct StreamerState {
  uint32_t status_raw = 0;
  uint32_t qout = 0;
  uint32_t qout_streamer = 0;
  StreamerOverrideState override_state;
};

enum class ClockSourceSelection {
  INT_CLK,
  EXT_CLK,
};

struct ClockPllState {
  std::string profile;
  std::optional<uint32_t> charge_pump;
  std::optional<uint32_t> bandwidth;
};

struct ClockConfigState {
  ClockSelectionOptions selection;
  std::string source_display = "startup default";
  bool source_managed = false;
  ClockPllState core;
  ClockPllState internal;
};

struct ClockConfigRequest {
  ClockSourceSelection source = ClockSourceSelection::INT_CLK;
  std::string core_profile;
  std::string int_profile;
};

struct ClockMeasurementState {
  double ext_clk_hz = 0.0;
  double int_clk_hz = 0.0;
  double streamer_clk_hz = 0.0;
  double core_clk_hz = 0.0;
};

struct ClockingState {
  ClockConfigState tracked;
  ClockMeasurementState measured;
};

enum class TriggerModeSelection {
  INT,
  EXT,
  MISC,
  ANY,
  ALL,
};

struct TriggerConfigRequest {
  TriggerModeSelection mode = TriggerModeSelection::INT;
  uint32_t invert_result = 0;
  uint32_t invert_int = 0;
  uint32_t invert_ext = 0;
  uint32_t invert_misc = 0;
  uint32_t mask_int = ~uint32_t {0};
  uint32_t mask_ext = ~uint32_t {0};
  uint32_t mask_misc = ~uint32_t {0};
};

struct TriggerConfigState {
  uint32_t mode = 0;
  uint32_t invert_result = 0;
  uint32_t invert_int = 0;
  uint32_t invert_ext = 0;
  uint32_t invert_misc = 0;
  uint32_t invert_aux = 0;
  uint32_t mask_int = ~uint32_t {0};
  uint32_t mask_ext = ~uint32_t {0};
  uint32_t mask_misc = ~uint32_t {0};
  uint32_t mask_aux = ~uint32_t {0};
};

struct StatusSnapshot {
  uint64_t seqno = 0;
  unsigned poll_ms = 100;
  uint8_t aux_raw = 0;
  uint32_t trig_raw = 0;
  ClockingState clocking;
  StreamerState streamer;
  int last_stream_rc = RC_OK;
  std::string stream_message = "idle";
  TriggerConfigState trigger_settings;
  std::string combiner_mode = "SEL1";
  PortState output;
  std::array<PortState, 4> inputs {};
  std::string last_action = "idle";
  std::string last_error;
};

struct CombinerRequest {
  comb_mode mode = comb_mode::SEL1;
  PortState output;
  std::array<PortState, 4> inputs {};
};

struct StreamResult {
  bool ok = false;
  int rc = RC_OK;
  int http_status = 200;
  std::string message;
};

struct StreamLaunchRequest {
  std::string sequence_text;
  std::optional<bool> force_trigger_override;
  bool check_readback = false;
};

struct ResetResult {
  bool ok = true;
  std::string message;
};
