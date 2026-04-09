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
#include <cstdint>
#include <exception>
#include <ifaddrs.h>
#include <iostream>
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
#include "host_runtime.hh"
#include "httplib.h"
#include "misc.hh"
#include "pio.hh"
#include "ppversion.hh"
#include "ppworkflow.hh"
#include "qout.hh"
#include "readback.hh"

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

struct StatusSnapshot {
  uint64_t seqno = 0;
  unsigned poll_ms = 100;
  uint8_t aux_raw = 0;
  uint32_t trig_raw = 0;
  std::array<uint32_t, 4> qout_values {};
  int last_stream_rc = RC_OK;
  std::string stream_message = "idle";
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
        out << "\\u" << std::hex << std::uppercase << int(static_cast<unsigned char>(ch) >> 4)
            << int(static_cast<unsigned char>(ch) & 0xF) << std::dec << std::nouppercase;
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
    return parse_uint32_t(value);
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
  out << "\"stream\":{";
  out << "\"last_rc\":" << status.last_stream_rc << ',';
  out << "\"message\":\"" << json_escape(status.stream_message) << "\"},";
  out << "\"streamers\":[";
  for (size_t i = 0; i < status.qout_values.size(); ++i) {
    if (i) out << ',';
    out << '{';
    out << "\"index\":" << (i + 1) << ',';
    out << "\"qout\":" << status.qout_values[i];
    out << '}';
  }
  out << "],";
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
      <h1>PulsePins Web GUI</h1>
      <div id="global-status" class="notice">Connecting…</div>
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
      </div>
      <div class="meta-row">
        <span><strong>Combiner:</strong> <span id="combiner-mode"></span></span>
        <span><strong>Last action:</strong> <span id="last-action"></span></span>
        <span><strong>Last error:</strong> <span id="last-error"></span></span>
      </div>
    </section>

    <section class="panel">
      <h2>Streamer Overrides</h2>
      <form id="qout-form" class="form-grid">
        <label>q1<input name="q1" value="0" placeholder="0, 0xff, 0b1010"></label>
        <label>q2<input name="q2" value="0" placeholder="0, 0xff, 0b1010"></label>
        <label>q3<input name="q3" value="0" placeholder="0, 0xff, 0b1010"></label>
        <label>q4<input name="q4" value="0" placeholder="0, 0xff, 0b1010"></label>
        <button type="submit">Apply qout</button>
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
            <label>Invert<input name="output_invert" value="0"></label>
            <label>Mask<input name="output_mask" value="0xffffffff"></label>
            <label>Force enabled<select name="output_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
            <label>Force value<input name="output_force_value" value="0"></label>
          </div>
        </div>

        <div class="ports-grid">
          <div class="subpanel">
            <h3>Input 1</h3>
            <div class="port-grid">
              <label>Invert<input name="in1_invert" value="0"></label>
              <label>Mask<input name="in1_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in1_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in1_force_value" value="0"></label>
            </div>
          </div>
          <div class="subpanel">
            <h3>Input 2</h3>
            <div class="port-grid">
              <label>Invert<input name="in2_invert" value="0"></label>
              <label>Mask<input name="in2_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in2_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in2_force_value" value="0"></label>
            </div>
          </div>
          <div class="subpanel">
            <h3>Input 3</h3>
            <div class="port-grid">
              <label>Invert<input name="in3_invert" value="0"></label>
              <label>Mask<input name="in3_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in3_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in3_force_value" value="0"></label>
            </div>
          </div>
          <div class="subpanel">
            <h3>Input 4</h3>
            <div class="port-grid">
              <label>Invert<input name="in4_invert" value="0"></label>
              <label>Mask<input name="in4_mask" value="0xffffffff"></label>
              <label>Force enabled<select name="in4_force_enabled"><option value="0">false</option><option value="1">true</option></select></label>
              <label>Force value<input name="in4_force_value" value="0"></label>
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

.panel, .subpanel {
  background: #111827;
  border: 1px solid #334155;
  border-radius: 10px;
  padding: 1rem;
  margin-bottom: 1rem;
}

.status-grid, .ports-grid, .form-grid, .port-grid {
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

.meta, .meta-row, .notice, .result-box {
  color: #cbd5e1;
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
  const qoutForm = document.getElementById('qout-form');
  const combinerForm = document.getElementById('combiner-form');
  const streamForm = document.getElementById('stream-form');
  let pollMs = 100;
  let hardwareBusy = false;

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
    setBusy(qoutForm, busy);
    setBusy(combinerForm, busy);
    setBusy(streamForm, busy);
  }

  function formOwnsFocus(form) {
    return form.contains(document.activeElement);
  }

  function populateQout(status) {
    if (formOwnsFocus(qoutForm)) return;
    status.streamers.forEach((streamer) => {
      qoutForm.querySelector(`[name="q${streamer.index}"]`).value = streamer.qout;
    });
  }

  function populateCombiner(status) {
    if (formOwnsFocus(combinerForm)) return;
    combinerForm.querySelector('[name="mode"]').value = status.combiner.mode;
    const output = status.combiner.output;
    combinerForm.querySelector('[name="output_invert"]').value = output.invert;
    combinerForm.querySelector('[name="output_mask"]').value = output.mask;
    combinerForm.querySelector('[name="output_force_enabled"]').value = output.force_enabled ? '1' : '0';
    combinerForm.querySelector('[name="output_force_value"]').value = output.force_value;
    status.combiner.inputs.forEach((input) => {
      const base = `in${input.index}`;
      combinerForm.querySelector(`[name="${base}_invert"]`).value = input.invert;
      combinerForm.querySelector(`[name="${base}_mask"]`).value = input.mask;
      combinerForm.querySelector(`[name="${base}_force_enabled"]`).value = input.force_enabled ? '1' : '0';
      combinerForm.querySelector(`[name="${base}_force_value"]`).value = input.force_value;
    });
  }

  function renderStatus(status) {
    document.getElementById('aux-bits').textContent = status.aux.bits;
    document.getElementById('aux-raw').textContent = `raw=${status.aux.raw}`;
    document.getElementById('trig-bits').textContent = status.trig.bits;
    document.getElementById('trig-raw').textContent = `raw=${status.trig.raw}`;
    document.getElementById('trig-flags').textContent = `enable=${status.trig.enable} force=${status.trig.force} reset=${status.trig.reset}`;
    document.getElementById('combiner-mode').textContent = status.combiner.mode;
    document.getElementById('last-action').textContent = status.last_action;
    document.getElementById('last-error').textContent = status.last_error || '(none)';
    document.getElementById('stream-state').textContent = `rc=${status.stream.last_rc} message=${status.stream.message}`;
    streamResult.textContent = status.stream.message;
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
      setGlobal(result.message || 'qout updated');
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
    streamers(input, fpga),
    qout_ctrl(input, verbosity, fpga),
    play_streamer(input, fpga),
    readback_path(input, fpga),
    counters(input, fpga),
    pio_aux(fpga.dev_lw, PIO_AUX_BASE),
    comb(qout_ctrl.cq)
  {
    snapshot.poll_ms = poll_ms;
  }

  ~WebGuiController() = default;

  StatusSnapshot get_status_copy() {
    auto status = read_status();
    status.seqno = snapshot.seqno;
    status.last_action = snapshot.last_action;
    status.last_error = snapshot.last_error;
    status.last_stream_rc = snapshot.last_stream_rc;
    status.stream_message = snapshot.stream_message;
    return status;
  }

  void apply_qout_overrides(const uint32_t q1, const uint32_t q2, const uint32_t q3, const uint32_t q4) {
    qout_ctrl.cq.report();
    streamers.s1.sc.qout_set(q1);
    streamers.s2.sc.qout_set(q2);
    streamers.s3.sc.qout_set(q3);
    streamers.s4.sc.qout_set(q4);
    publish_status(read_status(), "applied qout overrides", "");
  }

  void apply_combiner_config(const CombinerRequest &request) {
    comb.mode(request.mode);
    apply_port_locked(comb, 0, request.output);
    for (size_t i = 0; i < request.inputs.size(); ++i) {
      apply_port_locked(comb, static_cast<int>(i + 1), request.inputs[i]);
    }
    publish_status(read_status(), "applied combiner config", "");
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

      const int rc = send_and_trig(play_streamer.fifo,
                                   play_streamer.sc,
                                   readback_path,
                                   counters,
                                   sequence,
                                   request_input,
                                   force_trigger_request,
                                   verbosity);
      if (rc == RC_OK) {
        publish_status(read_status(), "streamed sequence", "", rc, "Sequence streamed successfully");
        return {true, rc, httplib::StatusCode::OK_200, "Sequence streamed successfully"};
      }

      const auto error = std::string("Streaming failed with rc=") + std::to_string(rc);
      publish_status(read_status(), "stream failed", error, rc, error);
      return {false, rc, httplib::StatusCode::InternalServerError_500, error};
    } catch (const std::exception &e) {
      publish_status(read_status(), "stream failed", e.what(), -1, e.what());
      throw;
    } catch (...) {
      publish_status(read_status(), "stream failed", "Unhandled non-standard exception in stream worker", -1,
                     "Unhandled non-standard exception in stream worker");
      throw;
    }
  }

  void set_last_error(const std::string &message) {
    snapshot.last_error = message;
    snapshot.seqno++;
  }

private:
  void publish_status(StatusSnapshot status,
                      const std::string &last_action,
                      const std::string &last_error,
                      const int last_stream_rc = RC_OK,
                      const std::string &stream_message = "idle") {
    snapshot.seqno += 1;
    status.seqno = snapshot.seqno;
    status.last_action = last_action;
    status.last_error = last_error;
    snapshot.last_stream_rc = last_stream_rc;
    snapshot.stream_message = stream_message;
    snapshot.last_action = last_action;
    snapshot.last_error = last_error;
    snapshot = std::move(status);
  }

  FPGA &fpga;
  const InputParser &input;
  const Verbosity &verbosity;
  const unsigned poll_ms;
  multistreamer streamers;
  qout qout_ctrl;
  streamer play_streamer;
  readback readback_path;
  counter counters;
  pio_in pio_aux;
  combiner_qout &comb;
  StatusSnapshot snapshot;

  PortState read_port_state(combiner_qout &comb, const int index, const uint32_t cfg) {
    PortState state;
    state.invert = comb.get_invert(index);
    state.mask = comb.get_mask(index);
    state.force_enabled = force_enabled(cfg, index);
    state.force_value = comb.get_force(index);
    return state;
  }

  StatusSnapshot read_status() {
    StatusSnapshot status;
    status.poll_ms = poll_ms;
    status.aux_raw = static_cast<uint8_t>(pio_aux.read() & 0xffU);
    status.trig_raw = fpga.trig_ext.read();
    status.qout_values = {
      streamers.s1.sc.get_qout(),
      streamers.s2.sc.get_qout(),
      streamers.s3.sc.get_qout(),
      streamers.s4.sc.get_qout(),
    };
    const auto cfg = comb.get_cfg();
    comb.cfg(cfg); // keep the software shadow aligned before force/value readback helpers mutate RB bits.
    status.combiner_mode = to_string(comb.get_mode());
    status.output = read_port_state(comb, 0, cfg);
    for (size_t i = 0; i < status.inputs.size(); ++i) {
      status.inputs[i] = read_port_state(comb, static_cast<int>(i + 1), cfg);
    }
    return status;
  }

  void apply_port_locked(combiner_qout &comb, const int port, const PortState &state) {
    comb.invert(port, state.invert);
    comb.mask(port, state.mask);
    if (state.force_enabled) {
      comb.force(port, state.force_value);
    } else {
      comb.value(port, state.force_value);
      comb.release_force(port);
    }
  }

};

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
//  install_fatal_signal_handlers();
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
      const auto q1 = parse_u32_param(req, "q1");
      const auto q2 = parse_u32_param(req, "q2");
      const auto q3 = parse_u32_param(req, "q3");
      const auto q4 = parse_u32_param(req, "q4");
      if (verbosity.veryverbose) {
        std::cout << "ppwebgui: parsed /api/qout parameters" << std::endl;
        std::cout << "ppwebgui action: apply qout" << std::endl;
        std::cout << "  q1=" << std::dec << q1 << std::endl;
        std::cout << "  q2=" << std::dec << q2 << std::endl;
        std::cout << "  q3=" << std::dec << q3 << std::endl;
        std::cout << "  q4=" << std::dec << q4 << std::endl;
      }
      controller.apply_qout_overrides(q1, q2, q3, q4);
      if (verbosity.veryverbose) {
        std::cout << "ppwebgui: applied /api/qout request" << std::endl;
      }
      respond_json(res, operation_json("Applied qout overrides", controller.get_status_copy()));
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
