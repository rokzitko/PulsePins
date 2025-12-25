// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

// Generic class for implementing SCPI interface

#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <map>
#include <deque>
#include <functional>
#include <sstream>
#include <algorithm>
#include <iostream>

using boost::asio::ip::tcp;

// --- SCPI Node ---
struct ScpiNode {
    std::string name;
    size_t min_len;
    std::map<std::string, std::shared_ptr<ScpiNode>> children;

    std::function<std::string(const std::string&)> set_handler;
    std::function<std::string()> query_handler;

    explicit ScpiNode(std::string n) : name(std::move(n)) {
        min_len = 0;
        for (char c : name) if (std::isupper(c)) ++min_len;
        if (min_len == 0) min_len = name.size();
    }

    bool matches(const std::string& token) const {
        if (token.size() < min_len) return false;
        std::string upname = name;
        std::transform(upname.begin(), upname.end(), upname.begin(), ::toupper);
        std::string uptok = token;
        std::transform(uptok.begin(), uptok.end(), uptok.begin(), ::toupper);
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

// --- Base session ---
class ScpiSessionBase : public std::enable_shared_from_this<ScpiSessionBase> {
public:
    explicit ScpiSessionBase(tcp::socket socket)
        : socket_(std::move(socket)) {
        root_ = std::make_shared<ScpiNode>("ROOT");
        sesr_ |= POWER_ON;
    }

    virtual ~ScpiSessionBase() = default;

    void start() { read(); }

protected:
    tcp::socket socket_;
    boost::asio::streambuf buffer_;
    std::shared_ptr<ScpiNode> root_;

    // Status system
    std::deque<std::string> error_queue_;
    uint8_t sesr_ = 0;
    uint8_t ese_mask_ = 0;
    uint8_t sre_mask_ = 0;
    uint8_t stb_ = 0;

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
    void handle_command(const std::string &line) {
        if (line.empty()) return;
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
                return;
            }
        }

        // Execute
        std::string resp;
        if (is_query) {
            if (node->query_handler) resp = node->query_handler();
            else push_error("Query error: not queryable", QUERY_ERROR);
        } else {
            if (node->set_handler) resp = node->set_handler(arg);
            else if (node->query_handler) push_error("Query form required", QUERY_ERROR);
            else push_error("Command error: no action", COMMAND_ERROR);
        }
        if (!resp.empty()) write(resp);
    }

    void read() {
        auto self(shared_from_this());
        boost::asio::async_read_until(socket_, buffer_, '\n',
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    std::istream is(&buffer_);
                    std::string line;
                    std::getline(is, line);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    line = trim(line);
                    handle_command(line);
                    read();
                }
            });
    }

    void write(const std::string &msg) {
        if (msg.empty()) return;
        auto self(shared_from_this());
        auto out = std::make_shared<std::string>(msg + "\n");
        boost::asio::async_write(socket_, boost::asio::buffer(*out),
            [this, self, out](boost::system::error_code, std::size_t) {});
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
    ScpiServerBase(boost::asio::io_context &io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        accept();
    }
protected:
    tcp::acceptor acceptor_;
    virtual std::shared_ptr<ScpiSessionBase> make_session(tcp::socket socket) = 0;

    void accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) make_session(std::move(socket))->start();
                accept();
            });
    }
};
