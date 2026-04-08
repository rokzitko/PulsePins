// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Embedded host-side web GUI server for PulsePins.

#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <exception>
#include <ifaddrs.h>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include "basic_multi_dma.hh"
#include "combiner.hh"
#include "counter.hh"
#include "host_runtime.hh"
#include "httplib.h"
#include "misc.hh"
#include "pio.hh"
#include "ppversion.hh"
#include "ppworkflow.hh"
#include "readback.hh"

namespace {

constexpr size_t MAX_FORM_BODY_BYTES = 64 * 1024;
constexpr size_t MAX_SEQUENCE_TEXT_BYTES = 32 * 1024;
constexpr int RC_CANCELLED = -2;

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
  bool stream_active = false;
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
  out << "\"active\":" << (status.stream_active ? "true" : "false") << ',';
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

int wait_to_complete_or_cancel(streamer_control &sc,
                               std::atomic<bool> &stop_requested,
                               const Verbosity &v,
                               const uint64_t max_cnt = 10000) {
  int rc = RC_OK;
  if (v.veryverbose) {
    std::cout << "Waiting for streamer to complete" << std::endl;
  }
  uint64_t cnt = 0;
  while (!(sc.done() || sc.buffer_error()) && cnt < max_cnt) {
    if (stop_requested.load()) {
      sc.stop(true);
      sc.reset();
      return RC_CANCELLED;
    }
    sleep_1ms();
    cnt++;
  }
  if (cnt == max_cnt && v.verbose) {
    std::cout << "wait_to_complete(): timeout exceeded while waiting for completion." << std::endl;
    rc = RC_TIMEOUT;
  }
  if (v.veryverbose) {
    sc.status_report();
  }
  return rc;
}

int run_post_execution_checks_or_cancel(streamer_control &sc,
                                        readback &rb,
                                        counter &ctr,
                                        const value_t final,
                                        const bool rb_failure,
                                        const InputParser &input,
                                        const Verbosity &v,
                                        int rc,
                                        std::atomic<bool> &stop_requested) {
  const int wait_rc = wait_to_complete_or_cancel(sc, stop_requested, v);
  if (wait_rc == RC_CANCELLED) {
    return RC_CANCELLED;
  }
  rc |= wait_rc;
  sleep_1ms();
  value_t final_qout = sc.get_qout();
  const bool match = final_qout == final;
  std::cout << "send_and_trig(): Final qout=" << hex8(final_qout) << " [" << dec13(final_qout) << "]";
  if (match) {
    std::cout << " OK" << std::endl;
  } else {
    if (!(envVarExists("PP_IGNORE_QOUT_FINAL") || input.exists("-pp_ignore_qout_final"))) {
      std::cout << red << " Mismatch: expecting " << hex8(final) << rst << std::endl;
      rc |= RC_ERROR_QOUT_FINAL;
    }
  }
  sc.statistics();
  if (sc.get_input_fifo1_ctr_in() != sc.get_input_fifo1_ctr_out()) {
    std::cout << red << "Mismatch in the streamer input FIFO1 detected." << rst << std::endl;
    rc |= RC_ERROR_FIFO_CTR;
  }
  if (sc.get_input_fifo2_ctr_in() != sc.get_input_fifo2_ctr_out()) {
    std::cout << red << "Mismatch in the streamer input FIFO2 detected." << rst << std::endl;
    rc |= RC_ERROR_FIFO_CTR;
  }
  if (sc.get_output_fifo_ctr_in() != sc.get_output_fifo_ctr_out()) {
    std::cout << red << "Mismatch in the streamer output FIFO detected." << rst << std::endl;
    rc |= RC_ERROR_FIFO_CTR;
  }
  if (sc.get_overflow()) {
    std::cout << red << "Input FIFO overflow detected." << rst << std::endl;
    rc |= RC_ERROR_OVERFLOW_FIFO;
  }
  if (rb.overflow()) {
    std::cout << red << "Readback overflow detected." << rst << std::endl;
    rc |= RC_ERROR_OVERFLOW_RB;
  }
  ctr.latch_all();
  ctr.short_report();
  if (sc.buffer_error()) {
    std::cout << red << "Buffer error detected." << rst << std::endl;
    rc |= RC_ERROR_BUFFER_ERROR;
  }
  const auto crc32sc = sc.get_crc32();
  const auto crc32rb = rb.get_crc32();
  std::cout << "send_and_trig(): CRC=0x" << std::hex << std::setw(8) << std::setfill('0') << crc32sc;
  const bool crc_ok = crc32sc == crc32rb;
  if (crc_ok) {
    std::cout << green << " OK" << rst << std::endl;
  } else {
    std::cout << red << " Mismatch in readback CRC. Got=0x" << std::hex << std::setw(8) << std::setfill('0') << crc32rb << rst << std::endl;
    rc |= RC_ERROR_CRC_MISMATCH;
  }
  if (crc_ok && rb_failure) {
    if (input.exists("-ignore_rb_error_if_crc_ok") || envVarExists("PP_IGNORE_RB_ERROR_IF_CRC_OK")) {
      rc &= ~RC_ERROR_CHECK;
    }
  }
  return rc;
}

template<typename Transport>
int webgui_send_and_trig(Transport &tr,
                         streamer_control &sc,
                         readback &rb,
                         counter &ctr,
                         Sequence &elements,
                         const InputParser &input,
                         const bool force_trigger,
                         const Verbosity &v,
                         std::atomic<bool> &stop_requested) {
  int rc = RC_OK;
  const value_t final = append_final_output(elements, input);
  dump_sequence(elements, v);
  transmit_sequence(tr, sc, elements, v);
  if (stop_requested.load()) {
    sc.stop(true);
    sc.reset();
    return RC_CANCELLED;
  }
  activate_trigger(sc, input, force_trigger, v);
  if (stop_requested.load()) {
    sc.stop(true);
    sc.reset();
    return RC_CANCELLED;
  }

  const double timeout = readback_timeout(input);
  bool rb_failure = run_readback_check_phase(rb, elements, input, v, identity, timeout, rc);
  run_readback_dump_phase(rb, input, v, timeout);

  if (input.exists("-dont_wait")) {
    return rc;
  }

  return run_post_execution_checks_or_cancel(sc, rb, ctr, final, rb_failure, input, v, rc, stop_requested);
}

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
        <label>q1<input name="q1" value="0"></label>
        <label>q2<input name="q2" value="0"></label>
        <label>q3<input name="q3" value="0"></label>
        <label>q4<input name="q4" value="0"></label>
        <button type="submit">Apply qout</button>
      </form>
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
        <div class="sequence-actions">
          <button type="submit">Start streaming</button>
          <button type="button" id="stop-stream">Stop</button>
        </div>
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

.sequence-actions {
  display: flex;
  gap: 0.75rem;
}

.sequence-actions button {
  flex: 1 1 auto;
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
  const stopStreamButton = document.getElementById('stop-stream');
  const qoutForm = document.getElementById('qout-form');
  const combinerForm = document.getElementById('combiner-form');
  const streamForm = document.getElementById('stream-form');
  let pollMs = 100;
  let streamActive = false;

  function setGlobal(message, isError = false) {
    globalStatus.textContent = message;
    globalStatus.classList.toggle('error', isError);
  }

  function setBusy(form, busy) {
    for (const element of form.elements) {
      element.disabled = busy;
    }
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
    streamActive = status.stream.active;
    document.getElementById('stream-state').textContent = `active=${status.stream.active} rc=${status.stream.last_rc} message=${status.stream.message}`;
    streamForm.querySelector('button[type="submit"]').disabled = status.stream.active;
    stopStreamButton.disabled = !status.stream.active;
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
    setBusy(qoutForm, true);
    try {
      const body = new URLSearchParams(new FormData(qoutForm));
      const result = await fetchJson('/api/qout', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      setGlobal(result.message || 'qout updated');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      setBusy(qoutForm, false);
    }
  });

  combinerForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    setBusy(combinerForm, true);
    try {
      const body = new URLSearchParams(new FormData(combinerForm));
      const result = await fetchJson('/api/combiner', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      setGlobal(result.message || 'combiner updated');
    } catch (error) {
      setGlobal(error.message, true);
    } finally {
      setBusy(combinerForm, false);
    }
  });

  streamForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    setBusy(streamForm, true);
    streamResult.textContent = 'Streaming…';
    try {
      const body = new URLSearchParams();
      body.set('sequence_text', streamForm.querySelector('[name="sequence_text"]').value);
      if (streamForm.querySelector('[name="force_trigger"]').checked) {
        body.set('force_trigger', '1');
      }
      if (streamForm.querySelector('[name="check_readback"]').checked) {
        body.set('check_readback', '1');
      }
      const result = await fetchJson('/api/stream', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      streamResult.textContent = result.message || 'Sequence completed';
      setGlobal(result.message || 'stream completed');
    } catch (error) {
      streamResult.textContent = error.message;
      setGlobal(error.message, true);
    } finally {
      setBusy(streamForm, false);
      if (streamActive) {
        streamForm.querySelector('button[type="submit"]').disabled = true;
      }
    }
  });

  stopStreamButton.addEventListener('click', async () => {
    stopStreamButton.disabled = true;
    try {
      const body = new URLSearchParams();
      const result = await fetchJson('/api/stop', { method: 'POST', body });
      if (result.status) renderStatus(result.status);
      streamResult.textContent = result.message || 'Stop requested';
      setGlobal(result.message || 'stop requested');
    } catch (error) {
      setGlobal(error.message, true);
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
    play_streamer(input, fpga),
    readback_path(input, fpga),
    counters(input, fpga),
    pio_aux(fpga.dev_lw, PIO_AUX_BASE),
    comb(fpga.dev_h2f, COMBINER_QOUT_BASE)
  {
    sample_status_once();
  }

  ~WebGuiController() {
    stop_sampler();
    wait_for_stream_worker();
  }

  StatusSnapshot get_status_copy() {
    std::lock_guard<std::mutex> lock(status_mutex);
    return snapshot;
  }

  void start_sampler() {
    stop_flag.store(false);
    sampler_thread = std::thread([this]() {
      while (!stop_flag.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
        if (stop_flag.load()) {
          break;
        }
        sample_status_once();
      }
    });
  }

  void stop_sampler() {
    stop_flag.store(true);
    if (sampler_thread.joinable()) {
      sampler_thread.join();
    }
  }

  void apply_qout_overrides(const uint32_t q1, const uint32_t q2, const uint32_t q3, const uint32_t q4) {
    std::lock_guard<std::mutex> lock(hw_mutex);
    streamers.s1.sc.qout_set(q1);
    streamers.s2.sc.qout_set(q2);
    streamers.s3.sc.qout_set(q3);
    streamers.s4.sc.qout_set(q4);
    publish_locked(capture_status_locked(), "applied qout overrides", "");
  }

  void apply_combiner_config(const CombinerRequest &request) {
    std::lock_guard<std::mutex> lock(hw_mutex);
    comb.mode(request.mode);
    apply_port_locked(0, request.output);
    for (size_t i = 0; i < request.inputs.size(); ++i) {
      apply_port_locked(static_cast<int>(i + 1), request.inputs[i]);
    }
    publish_locked(capture_status_locked(), "applied combiner config", "");
  }

  StreamResult request_stream_stop() {
    std::unique_lock<std::mutex> stream_lock(stream_mutex);
    join_finished_stream_worker_locked(stream_lock);
    if (!stream_worker_active) {
      return {false, 0, httplib::StatusCode::Conflict_409, "No stream is currently active"};
    }
    stop_requested.store(true);
    update_stream_metadata_locked(true, RC_OK, "stop requested", "stop requested", "");
    return {true, RC_OK, httplib::StatusCode::OK_200, "Stop requested"};
  }

  StreamResult start_stream_text_sequence(StreamLaunchRequest request) {
    std::cerr << "ppwebgui: start_stream_text_sequence entered" << std::endl;
    if (request.sequence_text.empty()) {
      throw BadRequest("Sequence text must not be empty");
    }

    std::cerr << "ppwebgui: start_stream_text_sequence before stream lock" << std::endl;
    std::unique_lock<std::mutex> stream_lock(stream_mutex);
    std::cerr << "ppwebgui: start_stream_text_sequence acquired stream lock" << std::endl;
    join_finished_stream_worker_locked(stream_lock);
    std::cerr << "ppwebgui: start_stream_text_sequence checked prior worker" << std::endl;
    if (stream_worker_active) {
      return {false, 0, httplib::StatusCode::Conflict_409, "Another stream request is already in flight"};
    }

    std::cerr << "ppwebgui: start_stream_text_sequence setting active state" << std::endl;
    stream_worker_active = true;
    stop_requested.store(false);
    std::cerr << "ppwebgui: launching background stream worker" << std::endl;
    update_stream_metadata_locked(true, RC_OK, "stream in progress", "streaming sequence", "");
    stream_worker = std::thread([this, request = std::move(request)]() mutable {
      run_stream_worker(std::move(request));
    });
    return {true, RC_OK, httplib::StatusCode::OK_200, "Sequence accepted and started"};
  }

  void wait_for_stream_worker() {
    std::unique_lock<std::mutex> stream_lock(stream_mutex);
    join_finished_stream_worker_locked(stream_lock);
    if (!stream_worker.joinable()) {
      return;
    }
    auto worker = std::move(stream_worker);
    stream_lock.unlock();
    worker.join();
  }

  void set_last_error(const std::string &message) {
    std::lock_guard<std::mutex> lock(status_mutex);
    snapshot.last_error = message;
    snapshot.seqno++;
  }

private:
  void run_stream_worker(StreamLaunchRequest request) {
    int rc = RC_OK;
    std::string message = "Sequence streamed successfully";
    std::string last_action = "streamed sequence";
    std::string last_error;

    try {
      std::cerr << "ppwebgui: stream worker started" << std::endl;
      std::stringstream sequence_stream(request.sequence_text);
      auto [sequence, parsed_force_trigger] = parse_sequence_from_stream(sequence_stream);
      std::cerr << "ppwebgui: sequence parsed, size=" << sequence.size() << std::endl;
      const bool force_trigger_request = request.force_trigger_override.value_or(parsed_force_trigger);

      InputParser request_input(std::vector<std::string>{});
      if (request.check_readback) {
        request_input.add("-check");
      }

      std::lock_guard<std::mutex> hw_lock(hw_mutex);
      std::cerr << "ppwebgui: acquired hw mutex, starting send_and_trig path" << std::endl;
      rc = webgui_send_and_trig(play_streamer.fifo,
                                play_streamer.sc,
                                readback_path,
                                counters,
                                sequence,
                                request_input,
                                force_trigger_request,
                                verbosity,
                                stop_requested);
      std::cerr << "ppwebgui: stream worker finished send path rc=" << rc << std::endl;
      if (rc == RC_CANCELLED) {
        message = "Stream cancelled";
        last_action = "stream cancelled";
      } else if (rc != RC_OK) {
        message = std::string("Streaming failed with rc=") + std::to_string(rc);
        last_action = "stream failed";
        last_error = message;
      }
    } catch (const std::exception &e) {
      rc = -1;
      message = e.what();
      last_action = "stream failed";
      last_error = message;
      std::cerr << "ppwebgui: stream worker caught exception: " << message << std::endl;
    } catch (...) {
      rc = -1;
      message = "Unhandled non-standard exception in stream worker";
      last_action = "stream failed";
      last_error = message;
      std::cerr << "ppwebgui: stream worker caught non-standard exception" << std::endl;
    }

    std::unique_lock<std::mutex> stream_lock(stream_mutex);
    stream_worker_active = false;
    std::cerr << "ppwebgui: publishing final stream state" << std::endl;
    update_stream_metadata_locked(false, rc, message, last_action, last_error);
  }

  void join_finished_stream_worker_locked(std::unique_lock<std::mutex> &stream_lock) {
    if (stream_worker_active || !stream_worker.joinable()) {
      return;
    }
    auto worker = std::move(stream_worker);
    stream_lock.unlock();
    worker.join();
    stream_lock.lock();
  }

  void update_stream_metadata_locked(const bool active,
                                     const int rc,
                                     const std::string &stream_message,
                                     const std::string &last_action,
                                     const std::string &last_error) {
    std::lock_guard<std::mutex> lock(status_mutex);
    snapshot.seqno += 1;
    snapshot.stream_active = active;
    snapshot.last_stream_rc = rc;
    snapshot.stream_message = stream_message;
    snapshot.last_action = last_action;
    snapshot.last_error = last_error;
  }
  FPGA &fpga;
  const InputParser &input;
  const Verbosity &verbosity;
  const unsigned poll_ms;
  std::mutex hw_mutex;
  std::mutex status_mutex;
  std::mutex stream_mutex;
  std::atomic<bool> stop_flag {false};
  std::atomic<bool> stop_requested {false};
  std::thread sampler_thread;
  std::thread stream_worker;
  bool stream_worker_active = false;
  multistreamer streamers;
  streamer play_streamer;
  readback readback_path;
  counter counters;
  pio_in pio_aux;
  combiner comb;
  StatusSnapshot snapshot;

  PortState read_port_state_locked(const int index, const uint32_t cfg) {
    PortState state;
    state.invert = comb.get_invert(index);
    state.mask = comb.get_mask(index);
    state.force_enabled = force_enabled(cfg, index);
    state.force_value = comb.get_force(index);
    return state;
  }

  StatusSnapshot capture_status_locked() {
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
    status.output = read_port_state_locked(0, cfg);
    for (size_t i = 0; i < status.inputs.size(); ++i) {
      status.inputs[i] = read_port_state_locked(static_cast<int>(i + 1), cfg);
    }
    return status;
  }

  void publish_locked(StatusSnapshot status, const std::string &last_action, const std::string &last_error) {
    std::lock_guard<std::mutex> lock(status_mutex);
    status.seqno = snapshot.seqno + 1;
    status.last_action = last_action;
    status.last_error = last_error;
    snapshot = std::move(status);
  }

  void apply_port_locked(const int port, const PortState &state) {
    comb.invert(port, state.invert);
    comb.mask(port, state.mask);
    if (state.force_enabled) {
      comb.force(port, state.force_value);
    } else {
      comb.value(port, state.force_value);
      comb.release_force(port);
    }
  }

  void sample_status_once() {
    std::lock_guard<std::mutex> hw_lock(hw_mutex);
    auto status = capture_status_locked();
    std::lock_guard<std::mutex> status_lock(status_mutex);
    status.seqno = snapshot.seqno + 1;
    status.last_action = snapshot.last_action;
    status.last_error = snapshot.last_error;
    status.stream_active = snapshot.stream_active;
    status.last_stream_rc = snapshot.last_stream_rc;
    status.stream_message = snapshot.stream_message;
    snapshot = std::move(status);
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
      require_form_post(req);
      controller.apply_qout_overrides(parse_u32_param(req, "q1"),
                                      parse_u32_param(req, "q2"),
                                      parse_u32_param(req, "q3"),
                                      parse_u32_param(req, "q4"));
      respond_json(res, operation_json("Applied qout overrides", controller.get_status_copy()));
    }));

    server.Post("/api/combiner", wrap([&](const httplib::Request &req, httplib::Response &res) {
      require_form_post(req);
      controller.apply_combiner_config(parse_combiner_request(req));
      respond_json(res, operation_json("Applied combiner config", controller.get_status_copy()));
    }));

    server.Post("/api/stream", wrap([&](const httplib::Request &req, httplib::Response &res) {
      std::cerr << "ppwebgui: entered /api/stream handler" << std::endl;
      require_form_post(req, MAX_FORM_BODY_BYTES);
      std::cerr << "ppwebgui: /api/stream content-type validated, body bytes=" << req.body.size() << std::endl;
      const std::optional<bool> force_trigger_override = req.has_param("force_trigger")
        ? std::optional<bool>(parse_bool_param(req, "force_trigger"))
        : std::nullopt;
      std::cerr << "ppwebgui: /api/stream parsed boolean flags" << std::endl;
      StreamLaunchRequest request;
      request.sequence_text = require_bounded_text_param(req, "sequence_text", MAX_SEQUENCE_TEXT_BYTES);
      request.force_trigger_override = force_trigger_override;
      request.check_readback = parse_bool_param(req, "check_readback");
      std::cerr << "ppwebgui: /api/stream parsed request, sequence bytes=" << request.sequence_text.size() << std::endl;
      const auto result = controller.start_stream_text_sequence(std::move(request));
      std::cerr << "ppwebgui: /api/stream start_stream_text_sequence returned" << std::endl;
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

    server.Post("/api/stop", wrap([&](const httplib::Request &req, httplib::Response &res) {
      require_form_post(req);
      const auto result = controller.request_stream_stop();
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

    controller.start_sampler();

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
