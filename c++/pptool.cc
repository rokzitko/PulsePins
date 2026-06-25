// SPDX-License-Identifier: MIT
// Copyright (c) 2025, 2026 Rok Zitko

// Main entry point for the `pptool` executable family.
//
// The binary is invoked either directly as `pptool` or via symlinked names such as
// `ppfg`, `ppcounter`, and `ppdelay`. The program name selects the command handler,
// which keeps startup policy centralized while preserving small tool-style entry points.
// Architectural overview lives in `c++/README.md` and `docs/docs/pptool.md`.

#include <iostream>
#include <map>
#include <thread>
#include <memory>

#include "ppmisc.hh"
#include "pptool_commands.hh"
#include "host_runtime.hh"
#include "ppversion.hh"
#include <limits>
#include "delay.hh"
#include "misc.hh"
#include "definitions.hh"
#include "stall_timeout.hh"

#define HAS_LUA
#define HAS_SERVER

#ifdef HAS_LUA
#include "lua.hh"
#endif

#ifdef HAS_SERVER
#include "ppserver.hh"
#endif

bool exit_flag = false; // for handling exit requests when waiting (-wait)

int main(int argc, char *argv[])
{
  try {
    HostRuntime rt(argc, argv, version);
    auto &progname = rt.progname;
    auto &input = rt.input;
    auto &v = rt.verbosity;
    auto &fpga = rt.get_fpga();
#ifdef HAS_LUA
    lua_processor luna(input, v, fpga);
    if (v.veryverbose) luna.test();
//  luna.process_line("print(\"hello\")");
#endif
#ifdef HAS_SERVER
    std::unique_ptr<LineServer> server;
    if (input.exists("-server")) {
      // Bind to a specific interface IP, e.g. "127.0.0.1" or "192.168.1.10".
      // If you want "all interfaces", use "0.0.0.0".
      const auto ip = input.get_string("-ip", "0.0.0.0");
      const auto port = input.get_uint32("-port", 5555);
      if (port > std::numeric_limits<uint16_t>::max())
        throw std::runtime_error("-port must be in range 0..65535");
      const auto protocol = input.exists("-udp") ? Proto::UDP : Proto::TCP;
      if (v.verbose)
        std::cout << "Binding server to " << (protocol == Proto::UDP ? "UDP" : "TCP") << " port " << port << " @ " << ip << std::endl;
      server = std::make_unique<LineServer>(ip, static_cast<uint16_t>(port), protocol,
                                            [&luna](const std::string &line){ luna.process_line(line); });
      server->start();
    }
#endif

    // User-facing mode selection happens here. Adding a new `pp...` command normally
    // means implementing a handler and registering it in this dispatch table.
    static const std::map<std::string, std::function<int(FPGA &, const InputParser&, const Verbosity&)>> actions{
      {"pptool", pptool},
      {"pptest", pptest},
      {"ppmstest", ppmstest},
      {"ppdmatest", ppdmatest},
      {"ppfg", ppfg},
      {"ppdelay", ppdelay},
      {"ppreset", ppreset},
      {"pptrig", pptrig},
      {"ppqout", ppqout},
      {"ppaux", ppaux},
      {"ppread", ppread},
      {"ppcounter", ppcounter},
      {"ppts", ppts},
      {"ppgpsdo", ppgpsdo},
      {"pptemp", pptemp},
      {"ppfreq", ppfreq},
      {"ppplay", ppplay},
      {"ppvcd", ppvcd},
      {"pphelloworld", pphelloworld}
    };

    int rc = RC_OK;
    if (auto it = actions.find(progname); it != actions.end()) {
      try {
        rc = it->second(fpga, input, v);
        std::cout << "All done, exiting with return code " << std::dec << rc << std::endl;
      }
      catch (const StallTimeout &e) {
        std::cout << "timeout: " << e.what() << std::endl;
        rc = RC_TIMEOUT;
      }
      catch (const char *e) {
        std::cout << "exception: " << e << std::endl;
        rc = RC_EXCEPTION;
      }
    } else {
      std::cerr << "Unknown program name: " << progname << "\n";
      std::cerr << "Available modes:";
      for (auto const& [name, _] : actions)
        std::cerr << " " << name;
      std::cerr << "\n";
      rc = RC_INVALID_ARG;
    }

    // `-wait` keeps the process alive after the command has completed so external
    // tooling can continue interacting with the initialized runtime.
    if (input.exists("-wait")) {
      std::cout << "Waiting for exit." << std::endl;
      while (!exit_flag)
        sleep_1ms();
      std::cout << "Exiting." << std::endl;
    }

#ifdef HAS_SERVER
    if (server) server->stop();
#endif

    // Optional exit delay is useful when the caller wants outputs and status to remain
    // stable for a short time after the main command path returns.
    double exit_delay = 0.0;
    if (envVarExists("PP_EXIT_DELAY"))
      exit_delay = envDouble("PP_EXIT_DELAY").value_or(1.0);
    if (input.exists("-exit_delay"))
      exit_delay = parse_time(input, "-exit_delay", "1.0");
    sleepd(exit_delay);

    if (input.exists("-ignore-errors") && (rc != 0)) {
      std::cout << "WARNING: Ignoring errors, return code reset to zero." << std::endl;
      rc = RC_OK;
    }

    return rc;
  } catch (const StallTimeout &e) {
    std::cerr << "Timeout: " << e.what() << "\n";
    return RC_TIMEOUT;
  } catch (const std::exception &e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return RC_EXCEPTION;
  } catch (...) {
    std::cerr << "Fatal: unknown exception\n";
    return RC_EXCEPTION;
  }
}
