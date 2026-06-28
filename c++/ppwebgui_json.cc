// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_json.hh"

#include <bitset>
#include <iomanip>
#include <sstream>

#include "combiner.hh"
#include "config.h"

namespace {

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

std::string trigger_mode_to_semantic_string(const TriggerConfigState &state) {
  switch (state.mode & MODE_MASK) {
  case static_cast<uint32_t>(trig_mode::INT):
    return "INT";
  case static_cast<uint32_t>(trig_mode::EXT):
    return "EXT";
  case static_cast<uint32_t>(trig_mode::MISC):
    return "MISC";
  case static_cast<uint32_t>(trig_mode::AND):
    return "ALL";
  case static_cast<uint32_t>(trig_mode::OR):
    return "ANY";
  default:
    return trigger_mode_to_string(state.mode);
  }
}

} // namespace

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
  out << "\"mode\":\"" << trigger_mode_to_semantic_string(status.trigger_settings) << "\",";
  out << "\"invert_result\":" << status.trigger_settings.invert_result << ',';
  out << "\"invert_int\":" << status.trigger_settings.invert_int << ',';
  out << "\"invert_ext\":" << status.trigger_settings.invert_ext << ',';
  out << "\"invert_misc\":" << status.trigger_settings.invert_misc << ',';
  out << "\"invert_aux\":" << status.trigger_settings.invert_aux << ',';
  out << "\"mask_int\":" << status.trigger_settings.mask_int << ',';
  out << "\"mask_ext\":" << status.trigger_settings.mask_ext << ',';
  out << "\"mask_misc\":" << status.trigger_settings.mask_misc << ',';
  out << "\"mask_aux\":" << status.trigger_settings.mask_aux << "},";
  out << "\"clocking\":{";
  out << "\"tracked\":{";
  out << "\"source\":\"" << (status.clocking.tracked.source_managed ? json_escape(status.clocking.tracked.source_display) : "unmanaged") << "\",";
  out << "\"source_display\":\"" << json_escape(status.clocking.tracked.source_display) << "\",";
  out << "\"source_managed\":" << (status.clocking.tracked.source_managed ? "true" : "false") << ',';
  out << "\"core_profile\":\"" << json_escape(status.clocking.tracked.core.profile) << "\",";
  out << "\"int_profile\":\"" << json_escape(status.clocking.tracked.internal.profile) << "\"},";
  out << "\"measured\":{";
  out << "\"ext_clk_hz\":" << status.clocking.measured.ext_clk_hz << ',';
  out << "\"int_clk_hz\":" << status.clocking.measured.int_clk_hz << ',';
  out << "\"streamer_clk_hz\":" << status.clocking.measured.streamer_clk_hz << ',';
  out << "\"core_clk_hz\":" << status.clocking.measured.core_clk_hz << "}},";
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

std::string operation_json(const std::string &message, const StatusSnapshot &status, const int rc) {
  std::ostringstream out;
  out << '{';
  out << "\"ok\":true,";
  out << "\"rc\":" << rc << ',';
  out << "\"message\":\"" << json_escape(message) << "\",";
  out << "\"status\":" << status_to_json(status);
  out << '}';
  return out.str();
}
