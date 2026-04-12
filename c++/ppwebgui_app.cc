// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_app.hh"

#include <stdexcept>

#include "fpga.hh"
#include "ppwebgui_server.hh"
#include "ppwebgui_service.hh"
#include "ppwebgui_service_api.hh"
#include "verbosity.hh"

int run_ppwebgui(FPGA &fpga, const WebGuiRuntimeConfig &config, const Verbosity &verbosity) {
  try {
    // Keep the hardware-owning controller anchored here for the full server lifetime.
    // Route/UI code may only talk to it indirectly through non-owning pointer/reference adapters.
    WebGuiController controller(fpga, config, verbosity);
    // Keep the adapter declared after the controller so it is destroyed first.
    auto service_handle = make_webgui_service(controller);
    auto &service = *service_handle;
    run_ppwebgui_server(service, config, verbosity);
  } catch (const std::exception &e) {
    std::cerr << "Fatal: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
