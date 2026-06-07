// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Generic class for implementing SCPI interface

#pragma once
#include <asio.hpp>
#include <memory>
#include <string>
#include <map>
#include <deque>
#include <cctype>
#include <exception>
#include <functional>
#include <sstream>
#include <algorithm>
#include <iostream>

using asio::ip::tcp;

// --- SCPI Node ---
struct ScpiNode {
    std::string name;
    size_t min_len;
    std::map<std::string, std::shared_ptr<ScpiNode>> children;

    std::function<std::string(const std::string&)> set_handler;
    std::function<std::string()> query_handler;

    explicit ScpiNode(std::string n) : name(std::move(n)) {
        min_len = 0;
        for (char c : name) {
            if (std::isupper(static_cast<unsigned char>(c))) ++min_len;
        }
        if (min_len == 0) min_len = name.size();
    }

    bool matches(const std::string& token) const {
        if (token.size() < min_len) return false;
        std::string upname = name;
        std::transform(upname.begin(), upname.end(), upname.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        std::string uptok = token;
        std::transform(uptok.begin(), uptok.end(), uptok.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return upname.compare(0, uptok.size(), uptok) == 0;
    }
};

// --- Status bits ---
enum SesrBits : uint8_t {
    OPERATION_COMPLETE = 1 << 0,
    QUERY_ERROR        = 1 << 2,
    DEVICE_ERROR       = 1 << 3,
    EXECUTION_ERROR    = 1 << 4,
    COMMAND_ERROR      = 1 << 5,
    USER_REQUEST       = 1 << 6,
    POWER_ON           = 1 << 7
};
enum StbBits : uint8_t {
    EVENT_STATUS_BIT   = 1 << 5,
    MESSAGE_AVAILABLE  = 1 << 4
};

class ScpiCloseSession : public std::exception {
public:
    const char *what() const noexcept override {
        return "SCPI session closed";
    }
};

class ScpiStopServer : public std::exception {
public:
    const char *what() const noexcept override {
        return "SCPI server stopped";
    }
};

enum class SessionAction {
    continue_reading,
    close_session,
    stop_server,
};

// --- Base session ---
class ScpiSessionBase : public std::enable_shared_from_this<ScpiSessionBase> {
public:
    explicit ScpiSessionBase(tcp::socket socket)
        : socket_(std::move(socket)),
          buffer_(max_line_bytes) {
        root_ = std::make_shared<ScpiNode>("ROOT");
        sesr_ |= POWER_ON;
    }

    virtual ~ScpiSessionBase() = default;

    void start() { read(); }

protected:
    static constexpr std::size_t max_line_bytes = 64 * 1024;

    tcp::socket socket_;
    asio::streambuf buffer_;
    std::shared_ptr<ScpiNode> root_;
    bool verbose = true;

    // Status system
    std::deque<std::string> error_queue_;
    std::deque<std::shared_ptr<std::string>> write_queue_;
    uint8_t sesr_ = 0;
    uint8_t ese_mask_ = 0;
    uint8_t sre_mask_ = 0;
    uint8_t stb_ = 0;
    bool write_in_progress_ = false;

    // For derived classes: add commands to tree
    void add_node(const std::vector<std::string>& path,
                  std::function<std::string(const std::string&)> set_handler = {},
                  std::function<std::string()> query_handler = {})
    {
        auto node = root_;
        for (size_t i = 0; i < path.size(); ++i) {
            std::string key = path[i];
            auto it = node->children.find(key);
            if (it == node->children.end()) {
                auto child = std::make_shared<ScpiNode>(key);
                node->children[key] = child;
                node = child;
            } else {
                node = it->second;
            }
            if (i == path.size() - 1) {
                node->set_handler = set_handler;
                node->query_handler = query_handler;
            }
        }
    }

    void push_error(const std::string& err, SesrBits bit) {
        if (error_queue_.size() > 10) error_queue_.pop_front();
        error_queue_.push_back(err);
        sesr_ |= bit;
    }

    void clear_status() {
        error_queue_.clear();
        sesr_ = 0;
        stb_ = 0;
    }

    // Parser & dispatcher
    SessionAction handle_command(const std::string &line) {
        if (line.empty()) return SessionAction::continue_reading;
        std::string cmd = line;

        // Split tokens by colon
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream iss(cmd);
        while (std::getline(iss, token, ':')) {
            if (!token.empty()) tokens.push_back(trim(token));
        }

        // Query?
        bool is_query = false;
        if (!tokens.empty() && !tokens.back().empty() && tokens.back().back() == '?') {
            is_query = true;
            tokens.back().pop_back();
        }

        // Argument?
        std::string arg;
        if (!tokens.empty()) {
            auto pos = tokens.back().find(' ');
            if (pos != std::string::npos) {
                arg = trim(tokens.back().substr(pos+1));
                tokens.back() = tokens.back().substr(0, pos);
            }
        }

        // Walk tree
        auto node = root_;
        for (const auto& t : tokens) {
            bool matched = false;
            for (auto &kv : node->children) {
                if (kv.second->matches(t)) {
                    node = kv.second;
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                push_error("Command error: unknown token '" + t + "'", COMMAND_ERROR);
                write("ERROR");
                return SessionAction::continue_reading;
            }
        }

        if (verbose) std::cout << "Executing [" << line << "]" << std::endl;

        std::string resp;
        try {
            if (is_query) {
                if (node->query_handler) resp = node->query_handler();
                else push_error("Query error: not queryable", QUERY_ERROR);
            } else {
                if (node->set_handler) resp = node->set_handler(arg);
                else if (node->query_handler) push_error("Query form required", QUERY_ERROR);
                else push_error("Command error: no action", COMMAND_ERROR);
            }
        } catch (const ScpiCloseSession &) {
            return SessionAction::close_session;
        } catch (const ScpiStopServer &) {
            return SessionAction::stop_server;
        } catch (const std::exception &e) {
            push_error(std::string("Execution error: ") + e.what(), EXECUTION_ERROR);
            if (verbose) std::cout << "Handler exception [" << e.what() << "]" << std::endl;
            write("ERROR");
            return SessionAction::continue_reading;
        } catch (...) {
            push_error("Device error: unhandled exception", DEVICE_ERROR);
            if (verbose) std::cout << "Handler exception [non-standard]" << std::endl;
            write("ERROR");
            return SessionAction::continue_reading;
        }

        if (verbose) std::cout << "Responding [" << resp << "]" << std::endl;
        if (!resp.empty()) write(resp);
        return SessionAction::continue_reading;
    }

    void read() {
        auto self(shared_from_this());
        asio::async_read_until(socket_, buffer_, '\n',
            [this, self](asio::error_code ec, std::size_t) {
                if (!ec) {
                    std::istream is(&buffer_);
                    std::string line;
                    std::getline(is, line);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    line = trim(line);
                    const auto action = handle_command(line);
                    if (action == SessionAction::continue_reading) {
                        read();
                    } else if (action == SessionAction::close_session) {
                        close_session();
                    } else {
                        close_session();
                        stop_server();
                    }
                } else if (ec != asio::error::operation_aborted) {
                    if (verbose) {
                        if (ec == asio::error::not_found && buffer_.size() >= max_line_bytes) {
                            std::cerr << "SCPI read error: line too long" << std::endl;
                        } else {
                            std::cerr << "SCPI read error: " << ec.message() << std::endl;
                        }
                    }
                    close_session();
                }
            });
    }

    void close_session() {
        asio::error_code ec;
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }

    void stop_server() {
        auto &context = static_cast<asio::io_context &>(socket_.get_executor().context());
        context.stop();
    }

    void write(const std::string &msg) {
        if (msg.empty()) return;
        write_queue_.push_back(std::make_shared<std::string>(msg + "\n"));
        if (!write_in_progress()) {
            start_next_write();
        }
    }

    bool write_in_progress() const noexcept {
        return write_in_progress_;
    }

    void start_next_write() {
        if (write_queue_.empty()) {
            write_in_progress_ = false;
            return;
        }

        write_in_progress_ = true;
        auto self(shared_from_this());
        auto out = write_queue_.front();
        asio::async_write(socket_, asio::buffer(*out),
            [this, self, out](asio::error_code ec, std::size_t) {
                write_queue_.pop_front();
                if (ec) {
                    write_in_progress_ = false;
                    if (verbose) {
                        std::cerr << "SCPI write error: " << ec.message() << std::endl;
                    }
                    close_session();
                    return;
                }
                start_next_write();
            });
    }

    // Utility
    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }
};

// --- Base server ---
class ScpiServerBase {
public:
    ScpiServerBase(asio::io_context &io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        accept();
    }
protected:
    tcp::acceptor acceptor_;
    virtual std::shared_ptr<ScpiSessionBase> make_session(tcp::socket socket) = 0;

    void accept() {
        acceptor_.async_accept(
            [this](asio::error_code ec, tcp::socket socket) {
                if (!ec) {
                    try {
                        make_session(std::move(socket))->start();
                    } catch (const std::exception &e) {
                        std::cerr << "SCPI session start error: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "SCPI session start error: unhandled non-standard exception" << std::endl;
                    }
                } else {
                    if (ec == asio::error::operation_aborted) {
                        return;
                    }
                    std::cerr << "SCPI accept error: " << ec.message() << std::endl;
                }
                accept();
            });
    }
};
