// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Embedded host-side web GUI server for PulsePins.
//
// Note: this file is currently built with `-fno-inline` via the Makefile due to
// an optimization-sensitive crash observed on the board with the current ARM
// toolchain. Keep the workaround local to `ppwebgui` until the underlying
// miscompile/UB is understood better.

#include <array>
#include <bitset>
#include <cerrno>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <ifaddrs.h>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#ifdef PPWEBGUI_ENABLE_BACKTRACE
#include <execinfo.h>
#endif
#include <net/if.h>
#include <netinet/in.h>
#ifdef PPWEBGUI_ENABLE_BACKTRACE
#include <signal.h>
#endif

#include "basic_multi_dma.hh"
#include "combiner.hh"
#include "counter.hh"
#include "freq_meter.hh"
#include "host_runtime.hh"
#include "httplib.h"
#include "misc.hh"
#include "pio.hh"
#include "ppversion.hh"
#include "ppworkflow.hh"
#include "qout.hh"
#include "readback.hh"
#include "startup.hh"
#include "trigger.hh"

namespace {

constexpr size_t MAX_FORM_BODY_BYTES = 64 * 1024;
constexpr size_t MAX_SEQUENCE_TEXT_BYTES = 32 * 1024;

struct BadRequest : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct PortState {
  uint32_t invert = 0;
  uint32_t mask = 0;
  bool force_enabled = false;
  uint32_t force_value = 0;
};

struct StreamerOverrideState {
  bool enabled = false;
  uint32_t value = 0;
};

struct StreamerState {
  uint32_t qout = 0;
  uint32_t qout_streamer = 0;
  StreamerOverrideState override_state;
};

struct TriggerConfigState {
  uint32_t mode = 0;
  uint32_t invert_result = 0;
  uint32_t invert_int = 0;
  uint32_t invert_ext = 0;
  uint32_t invert_misc = 0;
  uint32_t invert_aux = 0;
  uint32_t mask_int = 0;
  uint32_t mask_ext = 0;
  uint32_t mask_misc = 0;
  uint32_t mask_aux = 0;
};

struct StatusSnapshot {
  uint64_t seqno = 0;
  unsigned poll_ms = 100;
  uint8_t aux_raw = 0;
  uint32_t trig_raw = 0;
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
  int rc = 0;
  int http_status = httplib::StatusCode::OK_200;
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

std::string bits8(const uint8_t value) {
  return std::bitset<8>(value).to_string();
}

std::string trig_bits(const uint32_t value) {
  return std::bitset<WIDTH_TRIGGER>(value & TRIGGER_MASK).to_string();
}

bool trig_enable(const uint32_t value) {
  return value & (1U << PIO1_ENABLE);
}

bool trig_force(const uint32_t value) {
  return value & (1U << PIO1_FORCE);
}

bool trig_reset(const uint32_t value) {
  return value & (1U << PIO1_RESET);
}

TriggerConfigState trigger_config_from_input(const InputParser &input) {
  TriggerConfigState state;
  state.mode = 0;

  const auto opts = resolve_trigger_options(input);
  if (opts.mode) {
    switch (*opts.mode) {
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
    case TriggerModeOption::standard:
      state.mode = static_cast<uint32_t>(trig_mode::OR);
      state.invert_ext = ~uint32_t {0};
      break;
    }
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

CombinerRequest combiner_request_from_input(const InputParser &input) {
  CombinerRequest request;
  request.mode = comb_mode::SEL1;

  if (input.exists("-out_sel1")) request.mode = comb_mode::SEL1;
  else if (input.exists("-out_sel2")) request.mode = comb_mode::SEL2;
  else if (input.exists("-out_sel3")) request.mode = comb_mode::SEL3;
  else if (input.exists("-out_sel4")) request.mode = comb_mode::SEL4;
  else if (input.exists("-out_and")) request.mode = comb_mode::AND;
  else if (input.exists("-out_or")) request.mode = comb_mode::OR;
  else if (input.exists("-out_xor")) request.mode = comb_mode::XOR;
  else if (input.exists("-out_xnor")) request.mode = comb_mode::XNOR;
  else if (input.exists("-out_maj")) request.mode = comb_mode::MAJ;
  else if (input.exists("-out_block8")) request.mode = comb_mode::BLOCK8;
  else if (input.exists("-out_block16")) request.mode = comb_mode::BLOCK16;
  else if (input.exists("-out_sum12")) request.mode = comb_mode::SUM12;
  else if (input.exists("-out_sum1234")) request.mode = comb_mode::SUM1234;
  else if (input.exists("-out_diff12")) request.mode = comb_mode::DIFF12;

  auto parse_port = [&](const char *invert_name, const char *mask_name, const char *force_name) {
    PortState state;
    state.invert = input.exists(invert_name) ? parse_value(input, invert_name, "0") : 0;
    state.mask = input.exists(mask_name) ? parse_value(input, mask_name, "0xffffffff") : 0xffffffffU;
    state.force_enabled = input.exists(force_name);
    state.force_value = state.force_enabled ? parse_value(input, force_name, "0") : 0;
    return state;
  };

  request.output = parse_port("-invert_out", "-mask_out", "-force_out");
  request.inputs[0] = parse_port("-invert1", "-mask1", "-force1");
  request.inputs[1] = parse_port("-invert2", "-mask2", "-force2");
  request.inputs[2] = parse_port("-invert3", "-mask3", "-force3");
  request.inputs[3] = parse_port("-invert4", "-mask4", "-force4");
  return request;
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

std::string trigger_mode_to_string(const uint32_t mode) {
  switch (mode & MODE_MASK) {
  case 0: return "INT";
  case 1: return "EXT";
  case 2: return "MISC";
  case 3: return "AUX";
  case 4: return "AND";
  case 5: return "OR";
  case 6: return "XOR";
  default:
    return "UNKNOWN(" + std::to_string(mode & MODE_MASK) + ')';
  }
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

std::string json_escape(std::string_view input) {
  std::ostringstream out;
  for (const auto ch : input) {
    switch (ch) {
    case '\\': out << "\\\\"; break;
    case '"': out << "\\\""; break;
    case '\b': out << "\\b"; break;
    case '\f': out << "\\f"; break;
    case '\n': out << "\\n"; break;
    case '\r': out << "\\r"; break;
    case '\t': out << "\\t"; break;
    default:
      if (static_cast<unsigned char>(ch) < 0x20) {
        out << "\\u"
            << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
            << static_cast<unsigned>(static_cast<unsigned char>(ch))
            << std::dec << std::nouppercase << std::setfill(' ');
      } else {
        out << ch;
      }
      break;
    }
  }
  return out.str();
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

std::string parse_bind_ip(const InputParser &input) {
  return input.get_string("-ip", "0.0.0.0");
}

int parse_bind_port(const InputParser &input) {
  const auto port = std::stoi(input.get_string("-port", "4242"));
  if (port < 0 || port > 65535) {
    throw std::runtime_error("-port must be in range 0..65535");
  }
  return port;
}

unsigned parse_poll_ms(const InputParser &input) {
  const auto poll_ms = std::stoi(input.get_string("-poll_ms", "100"));
  if (poll_ms <= 0) {
    throw std::runtime_error("-poll_ms must be greater than zero");
  }
  return static_cast<unsigned>(poll_ms);
}

std::string status_to_json(const StatusSnapshot &status) {
  std::ostringstream out;
  out << '{';
  out << "\"seqno\":" << status.seqno << ',';
  out << "\"poll_ms\":" << status.poll_ms << ',';
  out << "\"aux\":{";
  out << "\"raw\":" << static_cast<unsigned>(status.aux_raw) << ',';
  out << "\"bits\":\"" << bits8(status.aux_raw) << "\"},";
  out << "\"trig\":{";
  out << "\"raw\":" << status.trig_raw << ',';
  out << "\"bits\":\"" << trig_bits(status.trig_raw) << "\",";
  out << "\"enable\":" << (trig_enable(status.trig_raw) ? "true" : "false") << ',';
  out << "\"force\":" << (trig_force(status.trig_raw) ? "true" : "false") << ',';
  out << "\"reset\":" << (trig_reset(status.trig_raw) ? "true" : "false") << "},";
  out << "\"trigger_settings\":{";
  out << "\"mode\":\"" << trigger_mode_to_string(status.trigger_settings.mode) << "\",";
  out << "\"invert_result\":" << status.trigger_settings.invert_result << ',';
  out << "\"invert_int\":" << status.trigger_settings.invert_int << ',';
  out << "\"invert_ext\":" << status.trigger_settings.invert_ext << ',';
  out << "\"invert_misc\":" << status.trigger_settings.invert_misc << ',';
  out << "\"invert_aux\":" << status.trigger_settings.invert_aux << ',';
  out << "\"mask_int\":" << status.trigger_settings.mask_int << ',';
  out << "\"mask_ext\":" << status.trigger_settings.mask_ext << ',';
  out << "\"mask_misc\":" << status.trigger_settings.mask_misc << ',';
  out << "\"mask_aux\":" << status.trigger_settings.mask_aux << "},";
  out << "\"stream\":{";
  out << "\"last_rc\":" << status.last_stream_rc << ',';
  out << "\"message\":\"" << json_escape(status.stream_message) << "\"},";
  out << "\"streamer\":{";
  out << "\"qout\":" << status.streamer.qout << ',';
  out << "\"qout_streamer\":" << status.streamer.qout_streamer << ',';
  out << "\"override\":{";
  out << "\"enabled\":" << (status.streamer.override_state.enabled ? "true" : "false") << ',';
  out << "\"value\":" << status.streamer.override_state.value << "}},";
  out << "\"combiner\":{";
  out << "\"mode\":\"" << status.combiner_mode << "\",";
  out << "\"output\":{";
  out << "\"invert\":" << status.output.invert << ',';
  out << "\"mask\":" << status.output.mask << ',';
  out << "\"force_enabled\":" << (status.output.force_enabled ? "true" : "false") << ',';
  out << "\"force_value\":" << status.output.force_value << "},";
  out << "\"inputs\":[";
  for (size_t i = 0; i < status.inputs.size(); ++i) {
    const auto &input = status.inputs[i];
    if (i) out << ',';
    out << '{';
    out << "\"index\":" << (i + 1) << ',';
    out << "\"invert\":" << input.invert << ',';
    out << "\"mask\":" << input.mask << ',';
    out << "\"force_enabled\":" << (input.force_enabled ? "true" : "false") << ',';
    out << "\"force_value\":" << input.force_value;
    out << '}';
  }
  out << "]},";
  out << "\"last_action\":\"" << json_escape(status.last_action) << "\",";
  out << "\"last_error\":\"" << json_escape(status.last_error) << "\"";
  out << '}';
  return out.str();
}

std::string operation_json(const std::string &message, const StatusSnapshot &status, const int rc = RC_OK) {
  std::ostringstream out;
  out << '{';
  out << "\"ok\":true,";
  out << "\"rc\":" << rc << ',';
  out << "\"message\":\"" << json_escape(message) << "\",";
  out << "\"status\":" << status_to_json(status);
  out << '}';
  return out.str();
}

const char *bool_text(const bool value) {
  return value ? "true" : "false";
}

#ifdef PPWEBGUI_ENABLE_BACKTRACE
void fatal_signal_handler(int sig) {
  void *frames[64];
  const int count = backtrace(frames, 64);
  std::cerr << "ppwebgui: fatal signal " << sig << std::endl;
  backtrace_symbols_fd(frames, count, STDERR_FILENO);
  _Exit(128 + sig);
}

void install_fatal_signal_handlers() {
  signal(SIGSEGV, fatal_signal_handler);
  signal(SIGABRT, fatal_signal_handler);
  signal(SIGBUS, fatal_signal_handler);
}
#else
void install_fatal_signal_handlers() {}
#endif

std::string socket_address_to_string(const sockaddr *addr) {
  std::array<char, INET6_ADDRSTRLEN> buf {};
  if (addr->sa_family == AF_INET) {
    const auto *in = reinterpret_cast<const sockaddr_in *>(addr);
    if (!inet_ntop(AF_INET, &in->sin_addr, buf.data(), buf.size())) {
      return {};
    }
    return buf.data();
  }
  if (addr->sa_family == AF_INET6) {
    const auto *in6 = reinterpret_cast<const sockaddr_in6 *>(addr);
    if (!inet_ntop(AF_INET6, &in6->sin6_addr, buf.data(), buf.size())) {
      return {};
    }
    return buf.data();
  }
  return {};
}

std::vector<std::string> discover_interface_urls(const int port) {
  ifaddrs *ifa = nullptr;
  if (getifaddrs(&ifa) != 0) {
    return {};
  }

  std::unordered_set<std::string> urls;
  for (auto *current = ifa; current != nullptr; current = current->ifa_next) {
    if (!current->ifa_addr || !(current->ifa_flags & IFF_UP) || (current->ifa_flags & IFF_LOOPBACK)) {
      continue;
    }
    if (current->ifa_addr->sa_family != AF_INET) {
      continue;
    }
    const auto address = socket_address_to_string(current->ifa_addr);
    if (address.empty()) {
      continue;
    }
    urls.insert("http://" + address + ':' + std::to_string(port));
  }

  freeifaddrs(ifa);
  return std::vector<std::string>(urls.begin(), urls.end());
}

void print_startup_urls(const std::string &bind_ip, const int actual_port) {
  std::cout << "ppwebgui running on http://" << bind_ip << ':' << actual_port << std::endl;
  if (bind_ip != "0.0.0.0") {
    return;
  }

  std::cout << "Listening on all interfaces." << std::endl;
  const auto urls = discover_interface_urls(actual_port);
  if (urls.empty()) {
    std::cout << "Reach it using this board's current IPv4 address on port " << actual_port << '.' << std::endl;
    return;
  }

  std::cout << "Reachable URLs:" << std::endl;
  for (const auto &url : urls) {
    std::cout << "  " << url << std::endl;
  }
}

const char *index_html = R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>PulsePins Web GUI</title>
  <link rel="stylesheet" href="/app.css">
</head>
<body>
  <main class="app-shell">
    <header class="app-header">
      <div>
        <h1>PulsePins Web GUI</h1>
        <div class="meta">Single-stream control, trigger monitoring, combiner setup, and sequence playback.</div>
      </div>
      <div class="header-actions">
        <button id="reset-button" type="button" class="secondary-button">Reset hardware</button>
        <div id="global-status" class="notice">Connecting…</div>
      </div>
    </header>

    <section class="panel">
      <h2>Live Status</h2>
      <div class="status-grid">
        <div>
          <div class="label">AUX</div>
          <div id="aux-bits" class="bits"></div>
          <div id="aux-raw" class="meta"></div>
        </div>
        <div>
          <div class="label">TRIG</div>
          <div id="trig-bits" class="bits"></div>
          <div id="trig-raw" class="meta"></div>
          <div id="trig-flags" class="meta"></div>
        </div>
        <div>
          <div class="label">Streamer</div>
          <div id="streamer-qout" class="meta mono"></div>
          <div id="streamer-qout-raw" class="meta mono"></div>
          <div id="streamer-override" class="meta mono"></div>
        </div>
      </div>
      <div class="meta-row">
        <span><strong>Combiner:</strong> <span id="combiner-mode"></span></span>
        <span><strong>Trigger mode:</strong> <span id="trigger-mode-summary"></span></span>
        <span><strong>Last action:</strong> <span id="last-action"></span></span>
        <span><strong>Last error:</strong> <span id="last-error"></span></span>
      </div>
    </section>

    <section class="panel">
      <h2>Trigger Settings</h2>
      <div class="settings-grid">
        <div class="setting"><div class="label">Mode</div><div id="trigger-mode" class="mono"></div></div>
        <div class="setting"><div class="label">Result invert</div><div id="trigger-invert-result" class="mono"></div></div>
        <div class="setting"><div class="label">INT invert</div><div id="trigger-invert-int" class="mono"></div></div>
        <div class="setting"><div class="label">EXT invert</div><div id="trigger-invert-ext" class="mono"></div></div>
        <div class="setting"><div class="label">MISC invert</div><div id="trigger-invert-misc" class="mono"></div></div>
        <div class="setting"><div class="label">AUX invert</div><div id="trigger-invert-aux" class="mono"></div></div>
        <div class="setting"><div class="label">INT mask</div><div id="trigger-mask-int" class="mono"></div></div>
        <div class="setting"><div class="label">EXT mask</div><div id="trigger-mask-ext" class="mono"></div></div>
        <div class="setting"><div class="label">MISC mask</div><div id="trigger-mask-misc" class="mono"></div></div>
        <div class="setting"><div class="label">AUX mask</div><div id="trigger-mask-aux" class="mono"></div></div>
      </div>
    </section>

    <section class="panel">
      <h2>Streamer Override</h2>
      <form id="qout-form" class="form-grid">
        <label>Enabled<select name="override_enabled"><option value="0">false</option><option value="1">true</option></select></label>
        <label>Override value<input name="override_value" value="0x0" placeholder="0x0"></label>
        <button type="submit">Apply override</button>
      </form>
      <div class="meta">Accepted integer formats: decimal (`42`), hex (`0xff`), binary (`0b1010`), octal (`077`), and Verilog-style literals like `8'hFF` or `'b1010`.</div>
    </section>

    <section class="panel">
      <h2>Output Combiner</h2>
      <form id="combiner-form" class="combiner-form">
        <label>Mode
          <select name="mode" id="combiner-mode-select">
            <option>SEL1</option>
            <option>SEL2</option>
            <option>SEL3</option>
            <option>SEL4</option>
            <option>AND</option>
            <option>OR</option>
            <option>XOR</option>
            <option>XNOR</option>
            <option>MAJ</option>
            <option>BLOCK8</option>
            <option>BLOCK16</option>
            <option>SUM12</option>
            <option>SUM1234</option>
            <option>DIFF12</option>
          </select>
        </label>

        <div class="subpanel">
          <h3>Output</h3>
          <div class="port-grid">
            <label>Invert<input name="output_invert" value="0x0"></label>
            <label>Mask<input name="output_mask" value="0xffffffff"></label>
            <label>Force enabled<select name="output_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
            <label>Force value<input name="output_force_value" value="0x0"></label>
          </div>
        </div>

        <div class="ports-grid">
          <div class="subpanel">
            <h3>Input 1</h3>
            <div class="port-grid">
              <label>Invert<input name="in1_invert" value="0x0"></label>
              <label>Mask<input name="in1_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in1_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in1_force_value" value="0x0"></label>
            </div>
          </div>
          <div class="subpanel">
            <h3>Input 2</h3>
            <div class="port-grid">
              <label>Invert<input name="in2_invert" value="0x0"></label>
              <label>Mask<input name="in2_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in2_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in2_force_value" value="0x0"></label>
            </div>
          </div>
          <div class="subpanel">
            <h3>Input 3</h3>
            <div class="port-grid">
              <label>Invert<input name="in3_invert" value="0x0"></label>
              <label>Mask<input name="in3_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in3_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in3_force_value" value="0x0"></label>
            </div>
          </div>
          <div class="subpanel">
            <h3>Input 4</h3>
            <div class="port-grid">
              <label>Invert<input name="in4_invert" value="0x0"></label>
              <label>Mask<input name="in4_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in4_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in4_force_value" value="0x0"></label>
            </div>
          </div>
        </div>

        <button type="submit">Apply combiner</button>
      </form>
    </section>

    <section class="panel">
      <h2>Sequence</h2>
      <form id="stream-form" class="sequence-form">
        <label>Sequence text
          <textarea name="sequence_text" rows="8">d 1 0x1
d 1 0x0
</textarea>
        </label>
        <label class="checkbox"><input type="checkbox" name="force_trigger"> Force trigger</label>
        <label class="checkbox"><input type="checkbox" name="check_readback"> Check readback</label>
        <button type="submit">Start streaming</button>
      </form>
      <div id="stream-state" class="meta"></div>
      <pre id="stream-result" class="result-box"></pre>
    </section>
  </main>
  <script src="/app.js"></script>
</body>
</html>
)HTML";

const char *app_css = R"CSS(:root {
  color-scheme: light dark;
  font-family: Inter, ui-sans-serif, system-ui, sans-serif;
}

body {
  margin: 0;
  background: #0f172a;
  color: #e2e8f0;
}

.app-shell {
  max-width: 1200px;
  margin: 0 auto;
  padding: 1rem;
}

.app-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
  margin-bottom: 1rem;
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 0.75rem;
}

.panel, .subpanel {
  background: #111827;
  border: 1px solid #334155;
  border-radius: 10px;
  padding: 1rem;
  margin-bottom: 1rem;
}

.status-grid, .ports-grid, .form-grid, .port-grid, .settings-grid {
  display: grid;
  gap: 0.75rem;
}

.status-grid {
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
}

.form-grid {
  grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
  align-items: end;
}

.ports-grid {
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
}

.settings-grid {
  grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
}

.port-grid {
  grid-template-columns: 1fr 1fr;
}

label {
  display: grid;
  gap: 0.35rem;
  font-size: 0.95rem;
}

input, select, textarea, button {
  box-sizing: border-box;
  width: 100%;
  border-radius: 8px;
  border: 1px solid #475569;
  background: #0f172a;
  color: inherit;
  padding: 0.6rem 0.75rem;
}

button {
  cursor: pointer;
  background: #1d4ed8;
  font-weight: 600;
}

.secondary-button {
  width: auto;
  background: #475569;
}

button:disabled {
  opacity: 0.6;
  cursor: wait;
}

.checkbox {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.checkbox input {
  width: auto;
}

.bits {
  font-family: ui-monospace, SFMono-Regular, monospace;
  font-size: 1.25rem;
  letter-spacing: 0.15em;
  margin: 0.4rem 0;
}

.mono {
  font-family: ui-monospace, SFMono-Regular, monospace;
}

.meta, .meta-row, .notice, .result-box {
  color: #cbd5e1;
}

.setting {
  background: #0f172a;
  border: 1px solid #334155;
  border-radius: 8px;
  padding: 0.75rem;
}

.meta-row {
  display: flex;
  gap: 1rem;
  flex-wrap: wrap;
  margin-top: 0.75rem;
}

.notice {
  padding: 0.6rem 0.8rem;
  border-radius: 8px;
  background: #1e293b;
}

.notice.error {
  background: #7f1d1d;
}

.result-box {
  min-height: 4rem;
  white-space: pre-wrap;
  word-break: break-word;
}

@media (max-width: 640px) {
  .app-header {
    align-items: flex-start;
    flex-direction: column;
  }

  .port-grid {
    grid-template-columns: 1fr;
  }
}
)CSS";

const char *app_js = R"JS((() => {
  const globalStatus = document.getElementById('global-status');
  const streamResult = document.getElementById('stream-result');
  const resetButton = document.getElementById('reset-button');
  const qoutForm = document.getElementById('qout-form');
  const combinerForm = document.getElementById('combiner-form');
  const streamForm = document.getElementById('stream-form');
  let pollMs = 100;
  let hardwareBusy = false;

  function formatHex(value, width = 8) {
    const normalized = Number(value) >>> 0;
    return `0x${normalized.toString(16).padStart(width, '0')}`;
  }

  function setText(id, value) {
    document.getElementById(id).textContent = value;
  }

  function setGlobal(message, isError = false) {
    globalStatus.textContent = message;
    globalStatus.classList.toggle('error', isError);
  }

  function setBusy(form, busy) {
    for (const element of form.elements) {
      element.disabled = busy;
    }
  }

  function setHardwareBusy(busy) {
    hardwareBusy = busy;
    resetButton.disabled = busy;
    setBusy(qoutForm, busy);
    setBusy(combinerForm, busy);
    setBusy(streamForm, busy);
  }

  function formOwnsFocus(form) {
    return form.contains(document.activeElement);
  }

  function populateQout(status) {
    if (formOwnsFocus(qoutForm)) return;
    qoutForm.querySelector('[name="override_enabled"]').value = status.streamer.override.enabled ? '1' : '0';
    qoutForm.querySelector('[name="override_value"]').value = formatHex(status.streamer.override.value);
  }

  function populateCombiner(status) {
    if (formOwnsFocus(combinerForm)) return;
    combinerForm.querySelector('[name="mode"]').value = status.combiner.mode;
    const output = status.combiner.output;
    combinerForm.querySelector('[name="output_invert"]').value = formatHex(output.invert);
    combinerForm.querySelector('[name="output_mask"]').value = formatHex(output.mask);
    combinerForm.querySelector('[name="output_force_enabled"]').value = output.force_enabled ? '1' : '0';
    combinerForm.querySelector('[name="output_force_value"]').value = formatHex(output.force_value);
    status.combiner.inputs.forEach((input) => {
      const base = `in${input.index}`;
      combinerForm.querySelector(`[name="${base}_invert"]`).value = formatHex(input.invert);
      combinerForm.querySelector(`[name="${base}_mask"]`).value = formatHex(input.mask);
      combinerForm.querySelector(`[name="${base}_force_enabled"]`).value = input.force_enabled ? '1' : '0';
      combinerForm.querySelector(`[name="${base}_force_value"]`).value = formatHex(input.force_value);
    });
  }

  function renderTriggerSettings(settings) {
    setText('trigger-mode', settings.mode);
    setText('trigger-invert-result', formatHex(settings.invert_result));
    setText('trigger-invert-int', formatHex(settings.invert_int));
    setText('trigger-invert-ext', formatHex(settings.invert_ext));
    setText('trigger-invert-misc', formatHex(settings.invert_misc));
    setText('trigger-invert-aux', formatHex(settings.invert_aux));
    setText('trigger-mask-int', formatHex(settings.mask_int));
    setText('trigger-mask-ext', formatHex(settings.mask_ext));
    setText('trigger-mask-misc', formatHex(settings.mask_misc));
    setText('trigger-mask-aux', formatHex(settings.mask_aux));
  }

  function renderStatus(status) {
    setText('aux-bits', status.aux.bits);
    setText('aux-raw', `raw=${formatHex(status.aux.raw, 2)}`);
    setText('trig-bits', status.trig.bits);
    setText('trig-raw', `raw=${formatHex(status.trig.raw)}`);
    setText('trig-flags', `enable=${status.trig.enable} force=${status.trig.force} reset=${status.trig.reset}`);
    setText('streamer-qout', `qout=${formatHex(status.streamer.qout)}`);
    setText('streamer-qout-raw', `streamer=${formatHex(status.streamer.qout_streamer)}`);
    setText('streamer-override', `override=${status.streamer.override.enabled} value=${formatHex(status.streamer.override.value)}`);
    setText('combiner-mode', status.combiner.mode);
    setText('trigger-mode-summary', status.trigger_settings.mode);
    setText('last-action', status.last_action);
    setText('last-error', status.last_error || '(none)');
    setText('stream-state', `rc=${status.stream.last_rc} message=${status.stream.message}`);
    streamResult.textContent = status.stream.message;
    renderTriggerSettings(status.trigger_settings);
    populateQout(status);
    populateCombiner(status);
    pollMs = status.poll_ms || 100;
  }

  async function fetchJson(url, options) {
    const response = await fetch(url, options);
    const data = await response.json();
    if (!response.ok || data.ok === false) {
      throw new Error(data.error || data.message || `HTTP ${response.status}`);
    }
    return data;
  }

  async function pollStatus() {
    if (hardwareBusy) {
      window.setTimeout(pollStatus, pollMs);
      return;
    }
    try {
      const status = await fetchJson('/api/status');
      renderStatus(status);
      setGlobal('Connected');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      window.setTimeout(pollStatus, pollMs);
    }
  }

  qoutForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const body = new URLSearchParams(new FormData(qoutForm));
    setHardwareBusy(true);
    try {
      const result = await fetchJson('/api/qout', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      setGlobal(result.message || 'override updated');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  combinerForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const body = new URLSearchParams(new FormData(combinerForm));
    setHardwareBusy(true);
    try {
      const result = await fetchJson('/api/combiner', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      setGlobal(result.message || 'combiner updated');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  streamForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const body = new URLSearchParams();
    body.set('sequence_text', streamForm.querySelector('[name="sequence_text"]').value);
    if (streamForm.querySelector('[name="force_trigger"]').checked) {
      body.set('force_trigger', '1');
    }
    if (streamForm.querySelector('[name="check_readback"]').checked) {
      body.set('check_readback', '1');
    }
    setHardwareBusy(true);
    streamResult.textContent = 'Streaming…';
    try {
      const result = await fetchJson('/api/stream', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      streamResult.textContent = result.message || 'Sequence completed';
      setGlobal(result.message || 'stream completed');
    } catch (error) {
      streamResult.textContent = error.message;
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  resetButton.addEventListener('click', async () => {
    const body = new URLSearchParams();
    setHardwareBusy(true);
    try {
      const result = await fetchJson('/api/reset', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      streamResult.textContent = result.message || 'Hardware reset completed';
      setGlobal(result.message || 'hardware reset completed');
    } catch (error) {
      streamResult.textContent = error.message;
      setGlobal(error.message, true);
    } finally {
      setHardwareBusy(false);
    }
  });

  pollStatus();
})();
)JS";

class WebGuiController {
public:
  WebGuiController(FPGA &fpga_, const InputParser &input_, const Verbosity &verbosity_, const unsigned poll_ms_) :
    fpga(fpga_),
    input(input_),
    verbosity(verbosity_),
    poll_ms(poll_ms_),
    qout_ctrl(input, verbosity, fpga),
    play_streamer(input, fpga),
    trigger_ctrl(input, fpga),
    readback_path(input, fpga),
    counters(input, fpga),
    pio_aux(fpga.dev_lw, PIO_AUX_BASE),
    comb(qout_ctrl.cq),
    trig_comb(trigger_ctrl.ct)
  {
    snapshot.poll_ms = poll_ms;
    snapshot.trigger_settings = trigger_config_from_input(input);
    combiner_base_config = combiner_request_from_input(input);
    snapshot.combiner_mode = to_string(combiner_base_config.mode);
    snapshot.output = combiner_base_config.output;
    snapshot.inputs = combiner_base_config.inputs;
    streamer_override.enabled = (play_streamer.sc.get_control() & QOUT_SELECT) == QOUT_SELECT;
    snapshot.streamer.override_state = streamer_override;
    snapshot.streamer.qout = streamer_override.value;
    snapshot.streamer.qout_streamer = 0;
  }

  ~WebGuiController() = default;

  StatusSnapshot get_status_copy() {
    auto lock = fpga.acquire_lock();
    return read_status_locked();
  }

  void apply_streamer_override(const StreamerOverrideState &state) {
    auto lock = fpga.acquire_lock();
    apply_streamer_override_locked(state);
    publish_action_locked("applied streamer override", "");
  }

  void apply_combiner_config(const CombinerRequest &request) {
    auto lock = fpga.acquire_lock();
    apply_combiner_config_locked(request);
    publish_action_locked("applied combiner config", "");
  }

  ResetResult reset_hardware() {
    auto lock = fpga.acquire_lock();
    const auto preserved_combiner = read_combiner_config_locked();
    const auto preserved_trigger = read_trigger_config_locked();
    const auto preserved_override = snapshot.streamer.override_state;

    rstmgr rm;
    rm.s2f_reset(verbosity.verbose);
    apply_fpga_startup_policy(fpga, input);
    pp_freq_meter(input, fpga).report();

    play_streamer.set_initial_value(input);
    fpga.output_enable(true);
    play_streamer.sc.reset();
    readback_path.reset();
    counters.reset_all();

    apply_trigger_config_locked(preserved_trigger);
    apply_combiner_config_locked(preserved_combiner);
    apply_streamer_override_locked(preserved_override);

    publish_action_locked("reset hardware", "");
    return {true, "Hardware reset completed and web settings restored"};
  }

  StreamResult stream_text_sequence(StreamLaunchRequest request) {
    if (request.sequence_text.empty()) {
      throw BadRequest("Sequence text must not be empty");
    }
    try {
      std::stringstream sequence_stream(request.sequence_text);
      auto [sequence, parsed_force_trigger] = parse_sequence_from_stream(sequence_stream);
      const bool force_trigger_request = request.force_trigger_override.value_or(parsed_force_trigger);

      InputParser request_input(std::vector<std::string>{});
      if (request.check_readback) {
        request_input.add("-check");
      }

      auto lock = fpga.acquire_lock();
      readback_path.reset();
      counters.reset_all();
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
        publish_stream_result_locked("streamed sequence", "", rc, "Sequence streamed successfully");
        return {true, rc, httplib::StatusCode::OK_200, "Sequence streamed successfully"};
      }

      const auto error = std::string("Streaming failed with rc=") + std::to_string(rc);
      publish_stream_result_locked("stream failed", error, rc, error);
      return {false, rc, httplib::StatusCode::InternalServerError_500, error};
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

  void set_last_error(const std::string &message) {
    auto lock = fpga.acquire_lock();
    snapshot.last_error = message;
    snapshot.seqno++;
  }

private:
  void publish_action_locked(
    const std::string &last_action,
    const std::string &last_error) {
    snapshot.seqno += 1;
    snapshot.last_action = last_action;
    snapshot.last_error = last_error;
  }

  void publish_stream_result_locked(const std::string &last_action,
                                    const std::string &last_error,
                                    const int last_stream_rc,
                                    const std::string &stream_message) {
    publish_action_locked(last_action, last_error);
    snapshot.last_stream_rc = last_stream_rc;
    snapshot.stream_message = stream_message;
  }

  FPGA &fpga;
  const InputParser &input;
  const Verbosity &verbosity;
  const unsigned poll_ms;
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

  PortState read_port_state_locked(combiner_qout &combiner, const int index, const uint32_t cfg) {
    PortState state;
    state.invert = combiner.get_invert(index);
    state.mask = combiner.get_mask(index);
    state.force_enabled = force_enabled(cfg, index);
    state.force_value = combiner.get_force(index);
    return state;
  }

  void sync_qout_combiner_shadow_locked() {
    const auto cfg = comb.get_cfg();
    comb.cfg(cfg);
  }

  void sync_trigger_combiner_shadow_locked() {
    const auto cfg = trig_comb.get_cfg();
    trig_comb.cfg(cfg);
  }

  CombinerRequest read_combiner_config_locked() {
    return combiner_base_config;
  }

  TriggerConfigState read_trigger_config_locked() {
    return snapshot.trigger_settings;
  }

  StatusSnapshot read_status_locked() {
    StatusSnapshot status = snapshot;
    status.poll_ms = poll_ms;
    return status;
  }

  void apply_port_locked(combiner_qout &combiner, const int port, const PortState &state) {
    combiner.invert(port, state.invert);
    combiner.mask(port, state.mask);
    if (state.force_enabled) {
      combiner.force(port, state.force_value);
    } else {
      combiner.value(port, state.force_value);
      combiner.release_force(port);
    }
  }

  void apply_streamer_override_locked(const StreamerOverrideState &state) {
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

  void apply_combiner_config_locked(const CombinerRequest &request) {
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

  void apply_trigger_config_locked(const TriggerConfigState &state) {
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

};

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

int main(int argc, char *argv[]) {
  install_fatal_signal_handlers();
  HostRuntime runtime(argc, argv, version);
  auto &input = runtime.input;
  auto &fpga = runtime.get_fpga();
  auto &verbosity = runtime.verbosity;

  try {
    const auto bind_ip = parse_bind_ip(input);
    const auto bind_port = parse_bind_port(input);
    const auto poll_ms = parse_poll_ms(input);

    WebGuiController controller(fpga, input, verbosity, poll_ms);

    httplib::Server server;
    server.Get("/", [](const httplib::Request &, httplib::Response &res) {
      res.set_content(index_html, "text/html; charset=utf-8");
    });
    server.Get("/app.css", [](const httplib::Request &, httplib::Response &res) {
      res.set_content(app_css, "text/css; charset=utf-8");
    });
    server.Get("/app.js", [](const httplib::Request &, httplib::Response &res) {
      res.set_content(app_js, "application/javascript; charset=utf-8");
    });

    server.Get("/api/status", [&](const httplib::Request &, httplib::Response &res) {
      respond_json(res, status_to_json(controller.get_status_copy()));
    });

    auto wrap = [&](auto handler) {
      return [&](const httplib::Request &req, httplib::Response &res) {
        try {
          handler(req, res);
        } catch (const BadRequest &e) {
          controller.set_last_error(e.what());
          respond_error(res, httplib::StatusCode::BadRequest_400, e.what());
        } catch (const std::exception &e) {
          controller.set_last_error(e.what());
          respond_error(res, httplib::StatusCode::InternalServerError_500, e.what());
        } catch (...) {
          controller.set_last_error("Unhandled non-standard exception");
          respond_error(res, httplib::StatusCode::InternalServerError_500, "Unhandled non-standard exception");
        }
      };
    };

    server.Post("/api/qout", wrap([&](const httplib::Request &req, httplib::Response &res) {
      if (verbosity.veryverbose) {
        std::cout << "ppwebgui: entered /api/qout" << std::endl;
      }
      require_form_post(req);
      const auto state = parse_streamer_override_request(req);
      if (verbosity.veryverbose) {
        std::cout << "ppwebgui: parsed /api/qout parameters" << std::endl;
        std::cout << "ppwebgui action: apply streamer override" << std::endl;
        std::cout << "  enabled=" << bool_text(state.enabled) << std::endl;
        std::cout << "  value=0x" << std::hex << state.value << std::dec << std::endl;
      }
      controller.apply_streamer_override(state);
      if (verbosity.veryverbose) {
        std::cout << "ppwebgui: applied /api/qout request" << std::endl;
      }
      respond_json(res, operation_json("Applied streamer override", controller.get_status_copy()));
    }));

    server.Post("/api/combiner", wrap([&](const httplib::Request &req, httplib::Response &res) {
      require_form_post(req);
      const auto request = parse_combiner_request(req);
      if (verbosity.veryverbose) {
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
      controller.apply_combiner_config(request);
      respond_json(res, operation_json("Applied combiner config", controller.get_status_copy()));
    }));

    server.Post("/api/reset", wrap([&](const httplib::Request &req, httplib::Response &res) {
      require_form_post(req);
      if (verbosity.veryverbose) {
        std::cout << "ppwebgui action: reset hardware" << std::endl;
      }
      const auto result = controller.reset_hardware();
      respond_json(res, operation_json(result.message, controller.get_status_copy()));
    }));

    server.Post("/api/stream", wrap([&](const httplib::Request &req, httplib::Response &res) {
      require_form_post(req, MAX_FORM_BODY_BYTES);
      const std::optional<bool> force_trigger_override = req.has_param("force_trigger")
        ? std::optional<bool>(parse_bool_param(req, "force_trigger"))
        : std::nullopt;
      StreamLaunchRequest request;
      request.sequence_text = require_bounded_text_param(req, "sequence_text", MAX_SEQUENCE_TEXT_BYTES);
      request.force_trigger_override = force_trigger_override;
      request.check_readback = parse_bool_param(req, "check_readback");
      if (verbosity.veryverbose) {
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
      const auto result = controller.stream_text_sequence(std::move(request));
      std::ostringstream body;
      body << "{\"ok\":" << (result.ok ? "true" : "false")
        << ",\"rc\":" << result.rc
        << ",\"message\":\"" << json_escape(result.message) << "\"";
      if (result.ok) {
        body << ",\"status\":" << status_to_json(controller.get_status_copy());
      }
      body << '}';
      respond_json(res, body.str(), result.http_status);
    }));

    int actual_port = bind_port;
    if (bind_port == 0) {
      actual_port = server.bind_to_any_port(bind_ip);
      if (actual_port <= 0) {
        throw std::runtime_error("Failed to bind ppwebgui to an auto-selected port");
      }
    } else if (!server.bind_to_port(bind_ip, bind_port)) {
      throw std::runtime_error("Failed to bind ppwebgui to requested address/port");
    }

    print_startup_urls(bind_ip, actual_port);

    if (!server.listen_after_bind()) {
      throw std::runtime_error("ppwebgui listener terminated unexpectedly");
    }
  } catch (const std::exception &e) {
    std::cerr << "Fatal: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
