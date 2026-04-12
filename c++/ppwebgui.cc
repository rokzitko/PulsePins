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
#include "ppwebgui_assets.hh"
#include "ppwebgui_http.hh"
#include "ppwebgui_json.hh"
#include "ppversion.hh"
#include "ppwebgui_service.hh"
#include "ppwebgui_service_api.hh"
#include "ppwebgui_types.hh"
#include "ppworkflow.hh"
#include "qout.hh"
#include "readback.hh"
#include "startup.hh"
#include "trigger.hh"

namespace {

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

    // Keep the hardware-owning controller anchored here for the full server lifetime.
    // Route/UI code may only talk to it indirectly through non-owning pointer/reference adapters.
    WebGuiController controller(fpga, input, verbosity, poll_ms);
    // Keep the adapter declared after the controller so it is destroyed first.
    auto service_handle = make_webgui_service(controller);
    auto &service = *service_handle;

    httplib::Server server;
    register_ppwebgui_routes(server, service, verbosity, index_html, app_css, app_js);

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
