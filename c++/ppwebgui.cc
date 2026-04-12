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
#include "ppwebgui_service.hh"
#include "ppwebgui_types.hh"
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

bool streamer_buffer_error(const uint32_t value) {
  return value & BUFFER_ERROR;
}

bool streamer_done(const uint32_t value) {
  return value & DONE;
}

bool streamer_triggered(const uint32_t value) {
  return value & TRIGGERED;
}

bool streamer_armed(const uint32_t value) {
  return value & ARMED;
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
  out << "\"message\":\"" << json_escape(status.stream_message) << "\",";
  out << "\"runtime\":{";
  out << "\"raw\":" << status.streamer.status_raw << ',';
  out << "\"buffer_error\":" << (streamer_buffer_error(status.streamer.status_raw) ? "true" : "false") << ',';
  out << "\"done\":" << (streamer_done(status.streamer.status_raw) ? "true" : "false") << ',';
  out << "\"triggered\":" << (streamer_triggered(status.streamer.status_raw) ? "true" : "false") << ',';
  out << "\"armed\":" << (streamer_armed(status.streamer.status_raw) ? "true" : "false") << "}},";
  out << "\"streamer\":{";
  out << "\"status_raw\":" << status.streamer.status_raw << ',';
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
        <div class="header-action-group">
          <button id="reset-button" type="button" class="secondary-button">Reset hardware</button>
          <div class="meta action-note">Resets FPGA-side state, then restores current ppwebgui-managed settings.</div>
        </div>
        <div id="global-status" class="notice">Connecting...</div>
      </div>
    </header>

    <section class="panel">
      <h2>Status Provenance</h2>
      <div class="legend-grid">
        <div class="legend-item">
          <span class="state-tag live-tag">live hardware</span>
          <div class="meta">Polled from stable hardware register paths.</div>
        </div>
        <div class="legend-item">
          <span class="state-tag tracked-tag">tracked by ppwebgui</span>
          <div class="meta">Controller-managed state restored after reset. Not reread live.</div>
        </div>
        <div class="legend-item">
          <span class="state-tag local-tag">local edit</span>
          <div class="meta">Browser-only form changes until you click Apply or Revert.</div>
        </div>
      </div>
      <div class="meta warning-text">If another tool changes trigger, combiner, or qout state after ppwebgui starts, tracked fields here can drift from live hardware.</div>
    </section>

    <section class="panel">
      <div class="panel-heading">
        <h2>Live Hardware</h2>
        <span class="state-tag live-tag">live</span>
      </div>
      <div class="panel-note">Only stable hardware readbacks are polled live.</div>
      <div class="status-grid">
        <div class="status-card">
          <div class="label">AUX</div>
          <div id="aux-bits" class="bits"></div>
          <div id="aux-raw" class="meta mono"></div>
        </div>
        <div class="status-card">
          <div class="label">TRIG</div>
          <div id="trig-bits" class="bits"></div>
          <div id="trig-raw" class="meta mono"></div>
          <div id="trig-flags" class="meta mono"></div>
        </div>
        <div class="status-card">
          <div class="label">Streamer runtime</div>
          <div id="stream-runtime-flags" class="meta mono"></div>
          <div id="stream-runtime-raw" class="meta mono"></div>
        </div>
      </div>
    </section>

    <section class="panel">
      <div class="panel-heading">
        <h2>Tracked by ppwebgui</h2>
        <span class="state-tag tracked-tag">tracked</span>
      </div>
      <div class="panel-note">These values come from ppwebgui's controller snapshot and are restored after reset.</div>
      <div class="status-grid">
        <div class="status-card">
          <div class="label">Displayed qout</div>
          <div id="streamer-qout" class="meta mono"></div>
        </div>
        <div class="status-card">
          <div class="label">Tracked idle streamer qout</div>
          <div id="streamer-qout-raw" class="meta mono"></div>
        </div>
        <div class="status-card">
          <div class="label">Output override</div>
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
      <div class="panel-heading">
        <h2>Trigger Settings</h2>
        <div class="heading-tags">
          <span class="state-tag tracked-tag">tracked</span>
          <span class="state-tag neutral-tag">read-only</span>
        </div>
      </div>
      <div class="panel-note">Restored by ppwebgui on reset. These settings are not polled live.</div>
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
      <div class="panel-heading">
        <h2>Output Override</h2>
        <div class="heading-tags">
          <span class="state-tag tracked-tag">tracked</span>
          <span id="qout-local-tag" class="state-tag local-tag hidden">local edit</span>
        </div>
      </div>
      <div id="qout-form-state" class="form-state">Tracked output-override values are shown below. Local edits stay in the browser until you click Apply or Revert.</div>
      <form id="qout-form" class="form-grid">
        <label>Enabled<select name="override_enabled"><option value="0">false</option><option value="1">true</option></select></label>
        <label>Override value<input name="override_value" value="0x0" placeholder="0x0"></label>
        <button type="submit">Apply override</button>
        <button id="qout-revert-button" type="button" class="secondary-button">Revert local edits</button>
      </form>
      <div class="meta">Manual final-output override, implemented through the combiner output-force path.</div>
      <div class="meta">Accepted integer formats: decimal (`42`), hex (`0xff`), binary (`0b1010`), octal (`077`), and Verilog-style literals like `8'hFF` or `'b1010`.</div>
    </section>

    <section class="panel">
      <div class="panel-heading">
        <h2>Output Combiner</h2>
        <div class="heading-tags">
          <span class="state-tag tracked-tag">tracked</span>
          <span id="combiner-local-tag" class="state-tag local-tag hidden">local edit</span>
        </div>
      </div>
      <div id="combiner-form-state" class="form-state">Tracked combiner values are shown below. Local edits stay in the browser until you click Apply or Revert.</div>
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
        <button id="combiner-revert-button" type="button" class="secondary-button">Revert local edits</button>
      </form>
    </section>

    <section class="panel">
      <h2>Sequence</h2>
      <div class="panel-note">Start streaming resets hardware first and appends the tracked idle qout as the final output.</div>
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

.app-shell h1 {
  margin: 0 0 0.25rem 0;
}

.panel > h2 {
  margin-top: 0;
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
  align-items: flex-start;
  justify-content: flex-end;
  flex-wrap: wrap;
  gap: 0.75rem;
}

.header-action-group {
  display: grid;
  gap: 0.4rem;
  justify-items: start;
}

.action-note {
  max-width: 24rem;
}

.panel, .subpanel {
  background: #111827;
  border: 1px solid #334155;
  border-radius: 10px;
  padding: 1rem;
  margin-bottom: 1rem;
}

.status-grid, .ports-grid, .form-grid, .port-grid, .settings-grid, .legend-grid {
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

.legend-grid {
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  margin-bottom: 0.75rem;
}

.port-grid {
  grid-template-columns: 1fr 1fr;
}

.panel-heading {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 0.75rem;
  flex-wrap: wrap;
  margin-bottom: 0.35rem;
}

.panel-heading h2 {
  margin: 0;
}

.panel-note {
  color: #cbd5e1;
  margin-bottom: 0.75rem;
}

.heading-tags {
  display: flex;
  gap: 0.5rem;
  flex-wrap: wrap;
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

.legend-item, .status-card, .setting, .form-state {
  background: #0f172a;
  border: 1px solid #334155;
  border-radius: 8px;
  padding: 0.75rem;
}

.label {
  font-weight: 600;
  margin-bottom: 0.35rem;
}

.meta-row {
  display: flex;
  gap: 1rem;
  flex-wrap: wrap;
  margin-top: 0.75rem;
}

.state-tag {
  display: inline-flex;
  align-items: center;
  border-radius: 999px;
  padding: 0.18rem 0.55rem;
  font-size: 0.78rem;
  font-weight: 600;
  letter-spacing: 0.02em;
  text-transform: lowercase;
}

.live-tag {
  background: #0f766e;
  color: #ccfbf1;
}

.tracked-tag {
  background: #334155;
  color: #e2e8f0;
}

.local-tag {
  background: #92400e;
  color: #fef3c7;
}

.neutral-tag {
  background: #3730a3;
  color: #e0e7ff;
}

.warning-text {
  color: #fbbf24;
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

.form-state {
  margin-bottom: 0.75rem;
}

.form-state.local-edit {
  border-color: #f59e0b;
  color: #fde68a;
}

.form-dirty {
  outline: 1px solid #f59e0b;
  outline-offset: 0.35rem;
  border-radius: 10px;
}

.hidden {
  display: none;
}

@media (max-width: 640px) {
  .app-header {
    align-items: flex-start;
    flex-direction: column;
  }

  .header-actions {
    width: 100%;
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
  const qoutRevertButton = document.getElementById('qout-revert-button');
  const combinerRevertButton = document.getElementById('combiner-revert-button');
  const qoutFormState = document.getElementById('qout-form-state');
  const combinerFormState = document.getElementById('combiner-form-state');
  const qoutLocalTag = document.getElementById('qout-local-tag');
  const combinerLocalTag = document.getElementById('combiner-local-tag');
  const qoutCleanText = 'Tracked output-override values are shown below. Local edits stay in the browser until you click Apply or Revert.';
  const combinerCleanText = 'Tracked combiner values are shown below. Local edits stay in the browser until you click Apply or Revert.';
  let pollMs = 100;
  let hardwareBusy = false;
  let qoutDirty = false;
  let combinerDirty = false;
  let lastStatus = null;

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

  function setFormDirty(form, dirty, stateElement, tagElement, cleanText, dirtyText) {
    form.classList.toggle('form-dirty', dirty);
    stateElement.classList.toggle('local-edit', dirty);
    stateElement.textContent = dirty ? dirtyText : cleanText;
    tagElement.classList.toggle('hidden', !dirty);
  }

  function setQoutDirty(dirty) {
    qoutDirty = dirty;
    setFormDirty(
      qoutForm,
      dirty,
      qoutFormState,
      qoutLocalTag,
      qoutCleanText,
      'Local edit only. This output-override form differs from the tracked ppwebgui state until you click Apply.');
  }

  function setCombinerDirty(dirty) {
    combinerDirty = dirty;
    setFormDirty(
      combinerForm,
      dirty,
      combinerFormState,
      combinerLocalTag,
      combinerCleanText,
      'Local edit only. This combiner form differs from the tracked ppwebgui state until you click Apply.');
  }

  function populateQout(status, force = false) {
    if (!force && qoutDirty) return;
    qoutForm.querySelector('[name="override_enabled"]').value = status.streamer.override.enabled ? '1' : '0';
    qoutForm.querySelector('[name="override_value"]').value = formatHex(status.streamer.override.value);
    setQoutDirty(false);
  }

  function populateCombiner(status, force = false) {
    if (!force && combinerDirty) return;
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
    setCombinerDirty(false);
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

  function renderStatus(status, options = {}) {
    lastStatus = status;
    const runtimeFlags = [];
    if (status.stream.runtime.buffer_error) runtimeFlags.push('buffer_error');
    if (status.stream.runtime.done) runtimeFlags.push('done');
    if (status.stream.runtime.triggered) runtimeFlags.push('triggered');
    if (status.stream.runtime.armed) runtimeFlags.push('armed');
    const runtimeSummary = runtimeFlags.length ? runtimeFlags.join(' ') : 'idle';

    setText('aux-bits', status.aux.bits);
    setText('aux-raw', `raw=${formatHex(status.aux.raw, 2)}`);
    setText('trig-bits', status.trig.bits);
    setText('trig-raw', `raw=${formatHex(status.trig.raw)}`);
    setText('trig-flags', `enable=${status.trig.enable} force=${status.trig.force} reset=${status.trig.reset}`);
    setText('stream-runtime-flags', `flags=${runtimeSummary}`);
    setText('stream-runtime-raw', `raw=${formatHex(status.stream.runtime.raw)}`);
    setText('streamer-qout', formatHex(status.streamer.qout));
    setText('streamer-qout-raw', formatHex(status.streamer.qout_streamer));
    setText('streamer-override', `enabled=${status.streamer.override.enabled} value=${formatHex(status.streamer.override.value)}`);
    setText('combiner-mode', status.combiner.mode);
    setText('trigger-mode-summary', status.trigger_settings.mode);
    setText('last-action', status.last_action);
    setText('last-error', status.last_error || '(none)');
    setText('stream-state', `last result rc=${status.stream.last_rc} message=${status.stream.message} live runtime=${runtimeSummary} raw=${formatHex(status.stream.runtime.raw)}`);
    streamResult.textContent = status.stream.message;
    renderTriggerSettings(status.trigger_settings);
    populateQout(status, options.forceQout === true);
    populateCombiner(status, options.forceCombiner === true);
    pollMs = status.poll_ms || 100;
  }

  function attachDirtyHandlers(form, markDirty) {
    const handler = (event) => {
      if (!event.isTrusted) return;
      markDirty(true);
    };
    form.addEventListener('input', handler);
    form.addEventListener('change', handler);
  }

  setQoutDirty(false);
  setCombinerDirty(false);
  attachDirtyHandlers(qoutForm, setQoutDirty);
  attachDirtyHandlers(combinerForm, setCombinerDirty);

  qoutRevertButton.addEventListener('click', () => {
    if (!lastStatus) return;
    populateQout(lastStatus, true);
  });

  combinerRevertButton.addEventListener('click', () => {
    if (!lastStatus) return;
    populateCombiner(lastStatus, true);
  });

  async function fetchJson(url, options = {}) {
    const response = await fetch(url, { cache: 'no-store', ...options });
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
      const status = await fetchJson(`/api/status?ts=${Date.now()}`);
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
      if (result.status) renderStatus(result.status, { forceQout: true });
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
      if (result.status) renderStatus(result.status, { forceCombiner: true });
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
    streamResult.textContent = 'Streaming...';
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
      res.set_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
      res.set_header("Pragma", "no-cache");
      res.set_header("Expires", "0");
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
      if (request.sequence_text.empty()) {
        throw BadRequest("Sequence text must not be empty");
      }
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
