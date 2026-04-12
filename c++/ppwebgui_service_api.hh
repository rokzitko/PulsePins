// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <memory>
#include <string>

#include "ppwebgui_types.hh"

class WebGuiController;

class WebGuiService {
public:
  virtual ~WebGuiService() = default;

  virtual StatusSnapshot get_status_copy() = 0;
  virtual void apply_streamer_override(const StreamerOverrideState &state) = 0;
  virtual void apply_combiner_config(const CombinerRequest &request) = 0;
  virtual ResetResult reset_hardware() = 0;
  virtual StreamResult stream_text_sequence(StreamLaunchRequest request) = 0;
  virtual void set_last_error(const std::string &message) = 0;
};

// Build a GUI/HTTP-facing adapter around an already-constructed hardware controller.
// The adapter must not take ownership of, copy, or move the controller because the
// underlying hardware wrappers are sensitive to lifetime and placement changes.
std::unique_ptr<WebGuiService> make_webgui_service(WebGuiController &controller);
