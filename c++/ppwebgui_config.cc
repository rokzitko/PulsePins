// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_config.hh"

#include <stdexcept>

#include "misc.hh"
#include "parser.hh"

namespace {

comb_mode comb_mode_from_input(const InputParser &input) {
  if (input.exists("-out_sel1")) return comb_mode::SEL1;
  if (input.exists("-out_sel2")) return comb_mode::SEL2;
  if (input.exists("-out_sel3")) return comb_mode::SEL3;
  if (input.exists("-out_sel4")) return comb_mode::SEL4;
  if (input.exists("-out_and")) return comb_mode::AND;
  if (input.exists("-out_or")) return comb_mode::OR;
  if (input.exists("-out_xor")) return comb_mode::XOR;
  if (input.exists("-out_xnor")) return comb_mode::XNOR;
  if (input.exists("-out_maj")) return comb_mode::MAJ;
  if (input.exists("-out_block8")) return comb_mode::BLOCK8;
  if (input.exists("-out_block16")) return comb_mode::BLOCK16;
  if (input.exists("-out_sum12")) return comb_mode::SUM12;
  if (input.exists("-out_sum1234")) return comb_mode::SUM1234;
  if (input.exists("-out_diff12")) return comb_mode::DIFF12;
  return comb_mode::SEL1;
}

PortState parse_port_from_input(const InputParser &input,
                                const char *invert_name,
                                const char *mask_name,
                                const char *force_name) {
  PortState state;
  state.invert = input.exists(invert_name) ? parse_value(input, invert_name, "0") : 0;
  state.mask = input.exists(mask_name) ? parse_value(input, mask_name, "0xffffffff") : 0xffffffffU;
  state.force_enabled = input.exists(force_name);
  state.force_value = state.force_enabled ? parse_value(input, force_name, "0") : 0;
  return state;
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

} // namespace

WebGuiRuntimeConfig resolve_webgui_runtime_config(const InputParser &input) {
  WebGuiRuntimeConfig config;
  config.bind_ip = parse_bind_ip(input);
  config.bind_port = parse_bind_port(input);
  config.poll_ms = parse_poll_ms(input);
  config.clock_selection = resolve_clock_selection_options(input);
  config.core_pll = resolve_core_pll_options(input);
  config.int_pll = resolve_int_pll_options(input);
  config.freq_meter_options = resolve_freq_meter_options(input);
  config.streamer_options = resolve_streamer_options(input);
  config.readback_options = resolve_readback_options(input);
  config.trigger_options = resolve_trigger_options(input);
  config.combiner_request.mode = comb_mode_from_input(input);
  config.combiner_request.output = parse_port_from_input(input, "-invert_out", "-mask_out", "-force_out");
  config.combiner_request.inputs[0] = parse_port_from_input(input, "-invert1", "-mask1", "-force1");
  config.combiner_request.inputs[1] = parse_port_from_input(input, "-invert2", "-mask2", "-force2");
  config.combiner_request.inputs[2] = parse_port_from_input(input, "-invert3", "-mask3", "-force3");
  config.combiner_request.inputs[3] = parse_port_from_input(input, "-invert4", "-mask4", "-force4");
  return config;
}

WebGuiServerBinding webgui_server_binding(const WebGuiRuntimeConfig &config) {
  return {config.bind_ip, config.bind_port};
}
