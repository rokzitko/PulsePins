// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_http.hh"

#include <cerrno>
#include <cstdlib>
#include <cctype>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "httplib.h"
#include "misc.hh"
#include "ppwebgui_json.hh"
#include "ppwebgui_service_api.hh"
#include "ppwebgui_types.hh"

namespace {

constexpr size_t MAX_FORM_BODY_BYTES = 64 * 1024;
constexpr size_t MAX_SEQUENCE_TEXT_BYTES = 32 * 1024;

struct BadRequest : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

const char *bool_text(const bool value) {
  return value ? "true" : "false";
}

comb_mode comb_mode_from_string(std::string mode) {
  for (auto &c : mode) {
    c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
  }
  if (mode == "SEL1") return comb_mode::SEL1;
  if (mode == "SEL2") return comb_mode::SEL2;
  if (mode == "SEL3") return comb_mode::SEL3;
  if (mode == "SEL4") return comb_mode::SEL4;
  if (mode == "AND") return comb_mode::AND;
  if (mode == "OR") return comb_mode::OR;
  if (mode == "XOR") return comb_mode::XOR;
  if (mode == "XNOR") return comb_mode::XNOR;
  if (mode == "MAJ") return comb_mode::MAJ;
  if (mode == "BLOCK8") return comb_mode::BLOCK8;
  if (mode == "BLOCK16") return comb_mode::BLOCK16;
  if (mode == "SUM12") return comb_mode::SUM12;
  if (mode == "SUM1234") return comb_mode::SUM1234;
  if (mode == "DIFF12") return comb_mode::DIFF12;
  throw BadRequest("Invalid combiner mode: " + mode);
}

uint32_t parse_u32_literal(std::string value) {
  value = trim(stripUnderscores(value));
  if (value.empty()) {
    throw std::invalid_argument("Empty integer string");
  }

  if (containsChar(value, '\'')) {
    const auto parsed = parseVerilogInt(value);
    if (parsed > std::numeric_limits<uint32_t>::max()) {
      throw std::out_of_range("Integer exceeds uint32_t range");
    }
    return static_cast<uint32_t>(parsed);
  }

  if (value.size() > 2 && value[0] == '0' && (value[1] == 'b' || value[1] == 'B')) {
    errno = 0;
    char *end = nullptr;
    const auto parsed = std::strtoul(value.c_str() + 2, &end, 2);
    if (end == value.c_str() + 2 || *end != '\0' || errno == ERANGE || parsed > std::numeric_limits<uint32_t>::max()) {
      throw std::invalid_argument("Invalid binary integer");
    }
    return static_cast<uint32_t>(parsed);
  }

  errno = 0;
  char *end = nullptr;
  const auto parsed = std::strtoul(value.c_str(), &end, 0);
  if (end == value.c_str() || *end != '\0' || errno == ERANGE || parsed > std::numeric_limits<uint32_t>::max()) {
    throw std::invalid_argument("Invalid integer");
  }
  return static_cast<uint32_t>(parsed);
}

void respond_json(httplib::Response &res, const std::string &body, const int status = httplib::StatusCode::OK_200) {
  res.status = status;
  res.set_content(body, "application/json");
}

void respond_error(httplib::Response &res, const int status, std::string_view error_text) {
  std::cerr << "ppwebgui: HTTP " << status << " error: " << error_text << std::endl;
  std::ostringstream body;
  body << "{\"ok\":false,\"error\":\"" << json_escape(error_text) << "\"}";
  respond_json(res, body.str(), status);
}

void require_form_post(const httplib::Request &req, const size_t max_body_bytes = MAX_FORM_BODY_BYTES) {
  const auto content_type = req.get_header_value("Content-Type");
  if (content_type.empty()) {
    throw BadRequest("Missing Content-Type header");
  }
  if (content_type.find("application/x-www-form-urlencoded") == std::string::npos) {
    throw BadRequest("Expected application/x-www-form-urlencoded request body");
  }
  if (req.body.size() > max_body_bytes) {
    throw BadRequest("Request body is too large");
  }
}

std::string require_param(const httplib::Request &req, const char *name) {
  if (!req.has_param(name)) {
    throw BadRequest(std::string("Missing parameter: ") + name);
  }
  return req.get_param_value(name);
}

std::string optional_param(const httplib::Request &req, const char *name, const std::string &def = "") {
  return req.has_param(name) ? req.get_param_value(name) : def;
}

std::string require_bounded_text_param(const httplib::Request &req, const char *name, const size_t max_bytes) {
  const auto value = require_param(req, name);
  if (value.size() > max_bytes) {
    throw BadRequest(std::string(name) + " exceeds the maximum supported size");
  }
  return value;
}

uint32_t parse_u32_param(const httplib::Request &req, const char *name, const std::string &def = "") {
  const auto value = def.empty() ? require_param(req, name) : optional_param(req, name, def);
  try {
    return parse_u32_literal(value);
  } catch (const std::exception &) {
    throw BadRequest(std::string("Invalid integer for ") + name + ": " + value);
  }
}

bool parse_bool_param(const httplib::Request &req, const char *name, const bool def = false) {
  const auto value = optional_param(req, name, def ? "1" : "0");
  try {
    return parse_bool(value);
  } catch (const std::exception &) {
    throw BadRequest(std::string("Invalid boolean for ") + name + ": " + value);
  }
}

StreamerOverrideState parse_streamer_override_request(const httplib::Request &req) {
  StreamerOverrideState state;
  state.enabled = parse_bool_param(req, "override_enabled");
  state.value = parse_u32_param(req, "override_value", "0x0");
  return state;
}

CombinerRequest parse_combiner_request(const httplib::Request &req) {
  CombinerRequest request;
  request.mode = comb_mode_from_string(require_param(req, "mode"));

  auto parse_port = [&](const std::string &prefix) {
    PortState state;
    state.invert = parse_u32_param(req, (prefix + "_invert").c_str());
    state.mask = parse_u32_param(req, (prefix + "_mask").c_str());
    state.force_enabled = parse_bool_param(req, (prefix + "_force_enabled").c_str());
    state.force_value = parse_u32_param(req, (prefix + "_force_value").c_str());
    return state;
  };

  request.output = parse_port("output");
  for (size_t i = 0; i < request.inputs.size(); ++i) {
    request.inputs[i] = parse_port("in" + std::to_string(i + 1));
  }
  return request;
}

} // namespace

void register_ppwebgui_routes(httplib::Server &server,
                              WebGuiService &service,
                              const WebGuiHttpOptions options,
                              const char *index_html,
                              const char *app_css,
                              const char *app_js) {
  auto *service_ptr = &service;

  server.Get("/", [index_html](const httplib::Request &, httplib::Response &res) {
    res.set_content(index_html, "text/html; charset=utf-8");
  });
  server.Get("/app.css", [app_css](const httplib::Request &, httplib::Response &res) {
    res.set_content(app_css, "text/css; charset=utf-8");
  });
  server.Get("/app.js", [app_js](const httplib::Request &, httplib::Response &res) {
    res.set_content(app_js, "application/javascript; charset=utf-8");
  });

  server.Get("/api/status", [service_ptr](const httplib::Request &, httplib::Response &res) {
    res.set_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    res.set_header("Pragma", "no-cache");
    res.set_header("Expires", "0");
    respond_json(res, status_to_json(service_ptr->get_status_copy()));
  });

  auto wrap = [service_ptr](auto handler) {
    return [service_ptr, handler = std::move(handler)](const httplib::Request &req, httplib::Response &res) mutable {
      try {
        handler(req, res);
      } catch (const BadRequest &e) {
        service_ptr->set_last_error(e.what());
        respond_error(res, httplib::StatusCode::BadRequest_400, e.what());
      } catch (const std::exception &e) {
        service_ptr->set_last_error(e.what());
        respond_error(res, httplib::StatusCode::InternalServerError_500, e.what());
      } catch (...) {
        service_ptr->set_last_error("Unhandled non-standard exception");
        respond_error(res, httplib::StatusCode::InternalServerError_500, "Unhandled non-standard exception");
      }
    };
  };

  server.Post("/api/qout", wrap([service_ptr, options](const httplib::Request &req, httplib::Response &res) {
    if (options.veryverbose) {
      std::cout << "ppwebgui: entered /api/qout" << std::endl;
    }
    require_form_post(req);
    const auto state = parse_streamer_override_request(req);
    if (options.veryverbose) {
      std::cout << "ppwebgui: parsed /api/qout parameters" << std::endl;
      std::cout << "ppwebgui action: apply streamer override" << std::endl;
      std::cout << "  enabled=" << bool_text(state.enabled) << std::endl;
      std::cout << "  value=0x" << std::hex << state.value << std::dec << std::endl;
    }
    service_ptr->apply_streamer_override(state);
    if (options.veryverbose) {
      std::cout << "ppwebgui: applied /api/qout request" << std::endl;
    }
    respond_json(res, operation_json("Applied streamer override", service_ptr->get_status_copy()));
  }));

  server.Post("/api/combiner", wrap([service_ptr, options](const httplib::Request &req, httplib::Response &res) {
    require_form_post(req);
    const auto request = parse_combiner_request(req);
    if (options.veryverbose) {
      std::cout << "ppwebgui action: apply combiner" << std::endl;
      std::cout << "  mode=" << to_string(request.mode) << std::endl;
      auto log_port = [](const char *label, const PortState &state) {
        std::cout << "  " << label
                  << " invert=" << state.invert
                  << " mask=" << state.mask
                  << " force_enabled=" << bool_text(state.force_enabled)
                  << " force_value=" << state.force_value << std::endl;
      };
      log_port("output", request.output);
      for (size_t i = 0; i < request.inputs.size(); ++i) {
        const auto label = std::string("input") + std::to_string(i + 1);
        log_port(label.c_str(), request.inputs[i]);
      }
    }
    service_ptr->apply_combiner_config(request);
    respond_json(res, operation_json("Applied combiner config", service_ptr->get_status_copy()));
  }));

  server.Post("/api/reset", wrap([service_ptr, options](const httplib::Request &req, httplib::Response &res) {
    require_form_post(req);
    if (options.veryverbose) {
      std::cout << "ppwebgui action: reset hardware" << std::endl;
    }
    const auto result = service_ptr->reset_hardware();
    respond_json(res, operation_json(result.message, service_ptr->get_status_copy()));
  }));

  server.Post("/api/stream", wrap([service_ptr, options](const httplib::Request &req, httplib::Response &res) {
    require_form_post(req, MAX_FORM_BODY_BYTES);
    const std::optional<bool> force_trigger_override = req.has_param("force_trigger")
      ? std::optional<bool>(parse_bool_param(req, "force_trigger"))
      : std::nullopt;
    StreamLaunchRequest request;
    request.sequence_text = require_bounded_text_param(req, "sequence_text", MAX_SEQUENCE_TEXT_BYTES);
    if (request.sequence_text.empty()) {
      throw BadRequest("Sequence text must not be empty");
    }
    request.force_trigger_override = force_trigger_override;
    request.check_readback = parse_bool_param(req, "check_readback");
    if (options.veryverbose) {
      std::cout << "ppwebgui action: start stream" << std::endl;
      std::cout << "  force_trigger_override="
                << (request.force_trigger_override ? bool_text(*request.force_trigger_override) : "(none)")
                << std::endl;
      std::cout << "  check_readback=" << bool_text(request.check_readback) << std::endl;
      std::cout << "  sequence_text:" << std::endl;
      std::cout << request.sequence_text;
      if (request.sequence_text.empty() || request.sequence_text.back() != '\n') {
        std::cout << std::endl;
      }
    }
    const auto result = service_ptr->stream_text_sequence(std::move(request));
    std::ostringstream body;
    body << "{\"ok\":" << (result.ok ? "true" : "false")
      << ",\"rc\":" << result.rc
      << ",\"message\":\"" << json_escape(result.message) << "\"";
    if (result.ok) {
      body << ",\"status\":" << status_to_json(service_ptr->get_status_copy());
    }
    body << '}';
    respond_json(res, body.str(), result.http_status);
  }));
}
