// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Minimal TCP and UDP server helpers used by PulsePins host tools.

#include <atomic>
#include <cerrno>
#include <cstring>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

enum class Proto { TCP, UDP };

inline std::string client_ip_string(int cfd) {
  sockaddr_storage ss{}; socklen_t slen = sizeof(ss);
  if (getpeername(cfd, (sockaddr*)&ss, &slen) != 0) return {};
  std::array<char, INET6_ADDRSTRLEN> buf{};
  if (ss.ss_family == AF_INET) {
    auto* s = (sockaddr_in*)&ss;
    if (!inet_ntop(AF_INET, &s->sin_addr, buf.data(), buf.size())) return {};
  } else if (ss.ss_family == AF_INET6) {
    auto* s = (sockaddr_in6*)&ss;
    if (!inet_ntop(AF_INET6, &s->sin6_addr, buf.data(), buf.size())) return {};
  } else {
    return {};
  }
  return std::string(buf.data());
}

static int make_listen_socket(const std::string& bind_ip, uint16_t port, Proto proto) {
  const int type = (proto == Proto::TCP) ? SOCK_STREAM : SOCK_DGRAM;
  int fd = ::socket(AF_INET, type, 0); // create a socket
  if (fd < 0) throw std::runtime_error("socket() failed: " + std::string(std::strerror(errno)));
  // Allow quick restart (TCP). Harmless for UDP.
  int yes = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    ::close(fd);
    throw std::runtime_error("setsockopt(SO_REUSEADDR) failed: " + std::string(std::strerror(errno)));
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (::inet_pton(AF_INET, bind_ip.c_str(), &addr.sin_addr) != 1) {
    ::close(fd);
    throw std::runtime_error("inet_pton() failed for bind_ip=" + bind_ip);
  }
  // bind a socket to an IP address
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    throw std::runtime_error("bind() failed: " + std::string(std::strerror(errno)));
  }
  if (proto == Proto::TCP) {
    // listen for connections on TCP port
    if (::listen(fd, 1) < 0) {
      ::close(fd);
      throw std::runtime_error("listen() failed: " + std::string(std::strerror(errno)));
    }
  }
  return fd;
}

// Reads from a stream socket and yields complete lines (newline-delimited).
// Handles CRLF by stripping a trailing '\r'.
static void read_lines_from_stream(int fd, const std::atomic<bool>& stop_flag,
                                   const std::function<void(const std::string&)>& on_line) {
  std::string buf;
  buf.reserve(4096);
  std::vector<char> tmp(4096);
  while (!stop_flag.load(std::memory_order_relaxed)) {
    ssize_t n = ::recv(fd, tmp.data(), tmp.size(), 0);
    if (n == 0) {
      // peer closed
      break;
    }
    if (n < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error("recv() failed: " + std::string(std::strerror(errno)));
    }
    buf.append(tmp.data(), static_cast<size_t>(n));
    // Extract full lines.
    size_t pos = 0;
    while (true) {
      size_t nl = buf.find('\n', pos);
      if (nl == std::string::npos) {
        // Keep remaining partial line.
        buf.erase(0, pos);
        break;
      }
      std::string line = buf.substr(pos, nl - pos);
      if (!line.empty() && line.back() == '\r') line.pop_back(); // handle CRLF
      on_line(line);
      pos = nl + 1;
    }
  }
}

// Reads UDP datagrams and treats each datagram as possibly containing multiple newline-delimited lines.
// (If your sender splits lines across datagrams, you need additional reassembly logic.)
static void read_lines_from_udp(int fd, const std::atomic<bool>& stop_flag,
                                const std::function<void(const std::string&)>& on_line) {
  std::vector<char> tmp(65536);
  while (!stop_flag.load(std::memory_order_relaxed)) {
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    ssize_t n = ::recvfrom(fd, tmp.data(), tmp.size(), 0,
                           reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (n < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error("recvfrom() failed: " + std::string(std::strerror(errno)));
    }
    std::string payload(tmp.data(), tmp.data() + n);
    size_t start = 0;
    while (start <= payload.size()) {
      size_t nl = payload.find('\n', start);
      if (nl == std::string::npos) nl = payload.size();
      std::string line = payload.substr(start, nl - start);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (!line.empty() || nl < payload.size()) { // allow empty lines if they are explicit
        on_line(line);
      }
      if (nl == payload.size()) break;
      start = nl + 1;
    }
  }
}

class LineServer {
public:
   using Handler = std::function<void(const std::string&)>;

   LineServer(std::string bind_ip, uint16_t port, Proto proto, Handler handler)
     : bind_ip_(std::move(bind_ip)), port_(port), proto_(proto), handler_(std::move(handler)) {}

   void start() {
     if (thread_.joinable()) throw std::runtime_error("Server already started");
     stop_flag_.store(false, std::memory_order_relaxed);
     thread_ = std::thread([this] { this->run(); });
   }

   void stop() {
     stop_flag_.store(true, std::memory_order_relaxed);
     // Best-effort unblock: close fds if open.
     // Note: closing from another thread is the common POSIX approach to unblock recv/accept.
     int fd = listen_fd_.exchange(-1);
     if (fd >= 0) {
       ::shutdown(fd, SHUT_RDWR);
       ::close(fd);
     }
     int cfd = client_fd_.exchange(-1);
     if (cfd >= 0) ::close(cfd);
     if (thread_.joinable()) thread_.join();
   }

   ~LineServer() {
     try { stop(); } catch (...) { /* no-throw */ }
   }

 private:
   void run() {
     try {
       const int lfd = make_listen_socket(bind_ip_, port_, proto_);
       listen_fd_.store(lfd);
       if (proto_ == Proto::TCP) {
         for (;;) {
           // Accept exactly one connection, then read lines until it closes or stop() is called.
           sockaddr_in peer{};
           socklen_t peer_len = sizeof(peer);
           int cfd = ::accept(lfd, reinterpret_cast<sockaddr*>(&peer), &peer_len);
           if (cfd < 0) {
             if (!stop_flag_.load(std::memory_order_relaxed)) {
               throw std::runtime_error("accept() failed: " + std::string(std::strerror(errno)));
             }
             return;
           }
           std::cout << "Connection from " << client_ip_string(cfd) << std::endl;
           client_fd_.store(cfd);
           // Optional: after accepting a single connection, you may close the listen socket.
           // That prevents any further connections.
           // int old_lfd = listen_fd_.exchange(-1);
           // if (old_lfd >= 0) ::close(old_lfd);
           read_lines_from_stream(cfd, stop_flag_, handler_);
           int old_cfd = client_fd_.exchange(-1);
           if (old_cfd >= 0) ::close(old_cfd);
           std::cout << "Connection closed." << std::endl;
         }
       } else {
         // UDP: no accept(). Just read datagrams and parse lines.
         read_lines_from_udp(lfd, stop_flag_, handler_);
         int old_lfd = listen_fd_.exchange(-1);
         if (old_lfd >= 0) ::close(old_lfd);
       }
     } catch (const std::exception& e) {
       // In real code, route this to your logging system.
       std::cerr << "LineServer error: " << e.what() << "\n";
     }
     std::cout << "LineServer terminating." << std::endl;
   }

   std::string bind_ip_;
   uint16_t port_;
   Proto proto_;
   Handler handler_;
   std::atomic<bool> stop_flag_{false};
   std::atomic<int> listen_fd_{-1};
   std::atomic<int> client_fd_{-1};
   std::thread thread_;
};
