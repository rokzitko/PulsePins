// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_app.hh"

#include <stdexcept>

#include "fpga.hh"
#include "httplib.h"
#include "ppwebgui_assets.hh"
#include "ppwebgui_bootstrap.hh"
#include "ppwebgui_http.hh"
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

    httplib::Server server;
    register_ppwebgui_routes(server, service, verbosity, index_html, app_css, app_js);

    int actual_port = config.bind_port;
    if (config.bind_port == 0) {
      actual_port = server.bind_to_any_port(config.bind_ip);
      if (actual_port <= 0) {
        throw std::runtime_error("Failed to bind ppwebgui to an auto-selected port");
      }
    } else if (!server.bind_to_port(config.bind_ip, config.bind_port)) {
      throw std::runtime_error("Failed to bind ppwebgui to requested address/port");
    }

    print_ppwebgui_startup_urls(config.bind_ip, actual_port);

    if (!server.listen_after_bind()) {
      throw std::runtime_error("ppwebgui listener terminated unexpectedly");
    }
  } catch (const std::exception &e) {
    std::cerr << "Fatal: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
