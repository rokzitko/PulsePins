// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

inline bool is_external_socket_bind(const std::string &bind_ip) {
  return bind_ip == "0.0.0.0";
}

inline void warn_if_external_socket_bind(const std::string &service,
                                        const std::string &bind_ip,
                                        const unsigned port,
                                        const std::string &interface_option = "-ip") {
  if (!is_external_socket_bind(bind_ip))
    return;

  const char *red = "\033[31m";
  const char *rst = "\033[0m";
  std::cout << red << "WARNING: " << service << " is listening on all interfaces ("
            << bind_ip << ':' << port << "), including external network interfaces. "
            << "Restrict the interface with " << interface_option
            << " <address> (for example 127.0.0.1) unless remote access is required."
            << rst << std::endl;
  std::cout << "waiting one second" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
}
