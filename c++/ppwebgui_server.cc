// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_server.hh"

#include <stdexcept>
#include <string>

#ifndef CPPHTTPLIB_FORM_URL_ENCODED_PAYLOAD_MAX_LENGTH
#define CPPHTTPLIB_FORM_URL_ENCODED_PAYLOAD_MAX_LENGTH (64 * 1024)
#endif
#include "httplib.h"
#include "ppwebgui_bootstrap.hh"
#include "ppwebgui_frontend.hh"
#include "ppwebgui_http.hh"
#include "ppwebgui_service_api.hh"

void run_ppwebgui_server(WebGuiService &service,
                         const WebGuiAssets &assets,
                         const WebGuiServerBinding &binding,
                         const WebGuiHttpOptions &http_options) {
  httplib::Server server;
  server.set_payload_max_length(64 * 1024);
  register_ppwebgui_routes(server, service, http_options, assets);

  int actual_port = binding.bind_port;
  if (binding.bind_port == 0) {
    actual_port = server.bind_to_any_port(binding.bind_ip);
    if (actual_port <= 0) {
      throw std::runtime_error("Failed to bind ppwebgui to " + binding.bind_ip + ":0");
    }
  } else if (!server.bind_to_port(binding.bind_ip, binding.bind_port)) {
    throw std::runtime_error("Failed to bind ppwebgui to " + binding.bind_ip + ":" + std::to_string(binding.bind_port));
  }

  print_ppwebgui_startup_urls(binding.bind_ip, actual_port);

  if (!server.listen_after_bind()) {
    throw std::runtime_error("ppwebgui listener terminated unexpectedly");
  }
}
