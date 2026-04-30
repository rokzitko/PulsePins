// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#include "ppwebgui_bootstrap.hh"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include <arpa/inet.h>
#ifdef PPWEBGUI_ENABLE_BACKTRACE
#include <execinfo.h>
#endif
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#ifdef PPWEBGUI_ENABLE_BACKTRACE
#include <signal.h>
#endif
#include <unistd.h>

namespace {

#ifdef PPWEBGUI_ENABLE_BACKTRACE
void fatal_signal_handler(int sig) {
  void *frames[64];
  const int count = backtrace(frames, 64);
  std::cerr << "ppwebgui: fatal signal " << sig << std::endl;
  backtrace_symbols_fd(frames, count, STDERR_FILENO);
  _Exit(128 + sig);
}
#endif

struct InterfaceAddress {
  std::string name;
  std::string address;
};

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

std::vector<InterfaceAddress> discover_interface_addresses() {
  ifaddrs *ifa = nullptr;
  if (getifaddrs(&ifa) != 0) {
    return {};
  }

  std::unordered_set<std::string> seen;
  std::vector<InterfaceAddress> addresses;
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
    const std::string key = std::string(current->ifa_name ? current->ifa_name : "") + '\n' + address;
    if (seen.insert(key).second) {
      addresses.push_back({current->ifa_name ? current->ifa_name : "(unknown)", address});
    }
  }

  freeifaddrs(ifa);
  return addresses;
}

} // namespace

void install_ppwebgui_fatal_signal_handlers() {
#ifdef PPWEBGUI_ENABLE_BACKTRACE
  signal(SIGSEGV, fatal_signal_handler);
  signal(SIGABRT, fatal_signal_handler);
  signal(SIGBUS, fatal_signal_handler);
#endif
}

void print_ppwebgui_startup_urls(const std::string &bind_ip, const int actual_port) {
  std::cout << "ppwebgui running on http://" << bind_ip << ':' << actual_port << std::endl;
  if (bind_ip != "0.0.0.0") {
    return;
  }

  std::cout << "Listening on all interfaces." << std::endl;

  const auto addresses = discover_interface_addresses();
  if (addresses.empty()) {
    std::cout << "Reach it using this board's current IPv4 address on port " << actual_port << '.' << std::endl;
    return;
  }

  std::cout << "Interface IPv4 addresses:" << std::endl;
  for (const auto &interface : addresses) {
    std::cout << "  " << interface.name << ": " << interface.address
              << "  http://" << interface.address << ':' << actual_port << std::endl;
  }
}
