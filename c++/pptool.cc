// SPDX-License-Identifier: MIT
// Copyright (c) 2025, 2026 Rok Zitko

// pptool main() is here...

#include "ppcommon.hh"
#include "ppmisc.hh"
#include "pptool_commands.hh"
#include "startup.hh"

#define HAS_LUA
#define HAS_SERVER

#ifdef HAS_LUA
#include "lua.hh"
#endif

#ifdef HAS_SERVER
#include "ppserver.hh"
#endif

#include "freq_meter.hh"

bool exit_flag = false; // for handling exit requests when waiting (-wait)

int main(int argc, char *argv[])
{
  auto progname = get_program_name(argc, argv);
  about(progname);
  InputParser input(argc, argv);
  auto v = set_verbosity(input);
  auto rt = bootstrap_process(v, version);
  FPGA fpga(v);
  apply_fpga_startup_policy(fpga, input);
  pp_freq_meter fm(input, fpga);
  fm.report();
#ifdef HAS_LUA
  lua_processor luna(input, v, fpga);
  if (v.veryverbose) luna.test();
//  luna.process_line("print(\"hello\")");
#endif
#ifdef HAS_SERVER
  std::unique_ptr<LineServer> server;
  std::thread server_thread;
  if (input.exists("-server")) {
    // Bind to a specific interface IP, e.g. "127.0.0.1" or "192.168.1.10".
    // If you want "all interfaces", use "0.0.0.0".
    const auto ip = input.get_string("-ip", "0.0.0.0");
    const auto port = input.get_uint32("-port", 5555);
    const auto protocol = input.exists("-udp") ? Proto::UDP : Proto::TCP;
    if (v.verbose)
      std::cout << "Binding server to " << (protocol == Proto::UDP ? "UDP" : "TCP") << " port " << port << " @ " << ip << std::endl;
    server = std::make_unique<LineServer>(ip, port, protocol,
                                          [&luna](const std::string &line){ luna.process_line(line); });
    server_thread = std::thread([&] { server->start(); });
  }
#endif

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
    {"ppvcd", ppvcd},
    {"pphelloworld", pphelloworld}
  };

  int rc = RC_OK;
  if (auto it = actions.find(progname); it != actions.end()) {
    try {
      rc = it->second(fpga, input, v);
      std::cout << "All done, exiting with return code " << std::dec << rc << std::endl;
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

  if (input.exists("-wait")) {  // wait forever
    std::cout << "Waiting for exit." << std::endl;
    while (!exit_flag)
      sleep_1ms();
    std::cout << "Exiting." << std::endl;
  }

  if (server) server->stop();
  if (server_thread.joinable()) server_thread.join();

  double exit_delay = 0.0;
  if (envVarExists("PP_EXIT_DELAY"))
    exit_delay = envDouble("PP_EXIT_DELAY").value_or(1.0);
  if (input.exists("-exit_delay"))
    exit_delay = parse_time(input, "-exit_delay", "1.0");
  sleep(exit_delay);

  if (input.exists("-ignore-errors") && (rc != 0)) {
    std::cout << "WARNING: Ignoring errors, return code reset to zero." << std::endl;
    rc = RC_OK;
  }

  return rc;
}
