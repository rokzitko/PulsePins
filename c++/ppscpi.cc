// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko
//
// SCPI server entry point for controlling PulsePins over Ethernet.

#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "scpi_server.hh"
#include "ppmisc.hh"
#include "ppversion.hh"
#include "ppworkflow.hh"
#include "verbosity.hh"
#include "freq_meter.hh"
#include "startup.hh"
#include "basic_multi_dma.hh"

class TerminateSession : public std::exception {
  std::string message;
public:
  explicit TerminateSession(const std::string& msg = "Session terminated")
    : message(msg) {}

  const char* what() const noexcept override {
    return message.c_str();
  }
};

class PPSession : public ScpiSessionBase {
public:
  explicit PPSession(tcp::socket socket, const InputParser &_input, FPGA &_fpga, const Verbosity &_v) :
    ScpiSessionBase(std::move(socket)),
    input(_input),
    fpga(_fpga),
    v(_v),
    s(input, fpga),
    rb(input, fpga),
    ctr(input, fpga)
    {
      build_tree();
    }
private:
  const InputParser &input;
  FPGA &fpga;
  const Verbosity &v;
  streamer s;
  readback rb;
  counter ctr;
  bool check = false;
  Sequence elements;
  bool force_trigger_p = false;

  void build_tree() {
    // *IDN?
    add_node({"*IDN"}, {}, [this]() {
      return "PulsePins,2025.11";
    });
    // *RST
    add_node({"*RST"}, [this](const std::string&) {
      check = false;
      elements.clear();
      force_trigger_p = false;
      return "";
    });
    // *CLS
    add_node({"*CLS"}, [this](const std::string&) {
      clear_status();
      return "";
    });
    // *OPC, *OPC?
    add_node({"*OPC"}, [this](const std::string&) {
      sesr_ |= OPERATION_COMPLETE;
      return "";
    }, [](){
      return "1";
    });
    // *ESR?
    add_node({"*ESR"}, {}, [this]() {
      uint8_t v = sesr_;
      sesr_ = 0;
      return std::to_string(v);
    });
    // *STB?
    add_node({"*STB"}, {}, [this]() {
      stb_ = 0;
      if (sesr_ & ese_mask_) stb_ |= EVENT_STATUS_BIT;
      if (!error_queue_.empty()) stb_ |= MESSAGE_AVAILABLE;
      return std::to_string(stb_);
    });
    // SYST:ERR?
    add_node({"SYSTem","ERRor"}, {}, [this]() {
      if (error_queue_.empty()) return std::string("0, \"No error\"");
      auto e = error_queue_.front(); error_queue_.pop_front();
      return "100, \"" + e + "\"";
    });
    // test1 (as in pptest)
    add_node({"TEST1"},
              [this](const std::string&) {
                Sequence elements;
                elements.push_back(el(1, 0b11));
                auto input1 = input;
                input1.add("-check");
                int rc = send_and_trig(s.fifo, s.sc, rb, ctr, elements, input1, force_trigger, v);
                return (rc ? "FAILURE" : "SUCCESS");
              });
    // Load a sequence
    add_node({"SEQ"},
              [this](const std::string &str) {
                std::stringstream s(str);
                auto result = parse_sequence_from_stream(s);
                elements = result.first;
                force_trigger_p = result.second;
                return "LOADED";
              });
    // Enable/disable readback checking
    add_node({"CHECK"},
              [this](const std::string& arg) {
                std::string up = arg; for (auto& c: up) c = toupper(c);
                if (up == "TRUE" || up == "1" || up == "ON")
                  check = true;
                else if (up == "FALSE" || up == "0" || up == "OFF") check = false;
                else push_error("Execution error: bad boolean option value", EXECUTION_ERROR);
                return "";
              },
              [this]() { return (check ? "TRUE" : "FALSE"); });
    // Stream out a sequence
    add_node({"STREAM"},
              [this](const std::string&) {
                auto input1 = input;
                if (check) input1.add("-check");
                int rc = send_and_trig(s.fifo, s.sc, rb, ctr, elements, input1, force_trigger_p, v); // to do: async?
                sesr_|=OPERATION_COMPLETE;
                return (rc == 0 ? "SUCCESS" : "FAILURE");
              });
    // *WAI
    add_node({"*WAI"}, [this](const std::string&) { return ""; });
    // Terminate ppserver program
    add_node({"TERMINATE"}, [this](const std::string&) { throw TerminateSession(); return ""; });
  }
};

// Derived server
class PPServer : public ScpiServerBase {
private:
  const InputParser &input;
  FPGA &fpga;
  const Verbosity &v;
public:
  PPServer(asio::io_context &io, unsigned short port, const InputParser &_input, FPGA &_fpga, const Verbosity &_v)
    : ScpiServerBase(io, port), input(_input), fpga(_fpga), v(_v) {}
protected:
  std::shared_ptr<ScpiSessionBase> make_session(tcp::socket socket) override {
    std::cout << "Connection from " << socket.remote_endpoint().address().to_string() << std::endl;
    return std::make_shared<PPSession>(std::move(socket), input, fpga, v);
  }
};

constexpr int server_port = 5025;

int main(int argc, char *argv[]) {
  about(get_program_name(argc, argv));
  InputParser input(argc, argv);
  auto v = set_verbosity(input);
  auto rt = bootstrap_process(v, version);
  FPGA fpga(v);
  apply_fpga_startup_policy(fpga, input);
  pp_freq_meter fm(input, fpga);
  fm.report();
  try {
    asio::io_context io;
    PPServer server(io, server_port, input, fpga, v);
    std::cout << "ppserver running on port " << server_port << std::endl;
    io.run();
  } catch (std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
