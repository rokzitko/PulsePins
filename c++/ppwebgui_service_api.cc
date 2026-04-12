// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_service_api.hh"

#include <memory>
#include <utility>

#include "ppwebgui_service.hh"

namespace {

class WebGuiServiceAdapter final : public WebGuiService {
public:
  explicit WebGuiServiceAdapter(WebGuiController &controller_) :
    controller(&controller_) {}

  StatusSnapshot get_status_copy() override {
    return controller->get_status_copy();
  }

  void apply_streamer_override(const StreamerOverrideState &state) override {
    controller->apply_streamer_override(state);
  }

  void apply_combiner_config(const CombinerRequest &request) override {
    controller->apply_combiner_config(request);
  }

  void apply_trigger_config(const TriggerConfigRequest &request) override {
    controller->apply_trigger_config(request);
  }

  ResetResult reset_hardware() override {
    return controller->reset_hardware();
  }

  StreamResult stream_text_sequence(StreamLaunchRequest request) override {
    return controller->stream_text_sequence(std::move(request));
  }

  void set_last_error(const std::string &message) override {
    controller->set_last_error(message);
  }

private:
  // Non-owning pointer by design: the controller instance is anchored in main and must keep
  // the same lifetime/location that the hardware-facing code was tested with.
  WebGuiController *controller;
};

} // namespace

std::unique_ptr<WebGuiService> make_webgui_service(WebGuiController &controller) {
  return std::unique_ptr<WebGuiService>(new WebGuiServiceAdapter(controller));
}
