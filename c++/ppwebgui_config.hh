// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <deque>
#include <string>

#include "options.hh"
#include "ppwebgui_types.hh"

struct WebGuiRuntimeConfig {
  std::string bind_ip = "0.0.0.0";
  int bind_port = 4242;
  unsigned poll_ms = 100;
  ClockSelectionOptions clock_selection;
  PllOptions core_pll;
  PllOptions int_pll;
  FreqMeterOptions freq_meter_options;
  StreamerOptions streamer_options;
  ReadbackOptions readback_options;
  TriggerOptions trigger_options;
  CombinerRequest combiner_request;
};

struct WebGuiServerBinding {
  std::string bind_ip = "0.0.0.0";
  int bind_port = 4242;
};

class InputParser;

WebGuiRuntimeConfig resolve_webgui_runtime_config(const InputParser &input);
WebGuiServerBinding webgui_server_binding(const WebGuiRuntimeConfig &config);
