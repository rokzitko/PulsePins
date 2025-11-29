// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Socket communications

#pragma once

#include <iostream>

#include <boost/asio.hpp>
using boost::asio::ip::tcp;
const unsigned short default_port = 555;

template <typename T>
  void accept_connections(T process, unsigned short port = default_port, std::ostream &f = std::cerr) {
    try {
      boost::asio::io_context io_context;
      tcp::acceptor acc(io_context, tcp::endpoint(tcp::v4(), port));
      for (;;) {
        f << "Waiting for connection on port " << std::dec << port << std::endl;
        process(acc.accept());
      }
    }
    catch (const char *e) {
      f << "exception: " << e << std::endl;
    }
  }

// Returns true if successful, false otherwise. This allows one to gracefully handle
// broken connections.
bool send_ram_block(ram_block &rb, tcp::socket &sock)
{
  mm data(rb.get_addr(), rb.get_size());
  auto buffer_addr = data.get_ptr(0);
  try {
    boost::asio::write(sock, boost::asio::buffer(buffer_addr, rb.get_size())); // write() is a blocking call
  }
  catch (boost::system::system_error &e) {
    boost::system::error_code ec = e.code();
    std::cout << "exception: " << ec.value() << " = " << ec.message() << " - exiting." << std::endl;
    return false;
  }
  return true;
}
