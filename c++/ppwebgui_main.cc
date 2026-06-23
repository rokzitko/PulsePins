// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Embedded host-side web GUI server for PulsePins.
//
#include "host_runtime.hh"
#include "definitions.hh"
#include "ppwebgui_app.hh"
#include "ppwebgui_bootstrap.hh"
#include "ppwebgui_config.hh"
#include "ppversion.hh"

#include <exception>
#include <iostream>

int main(int argc, char *argv[]) {
  try {
    install_ppwebgui_fatal_signal_handlers();
    HostRuntime runtime(argc, argv, version);
    const auto config = resolve_webgui_runtime_config(runtime.input);
    return run_ppwebgui(runtime.get_fpga(), config, runtime.verbosity);
  } catch (const std::exception &e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return RC_EXCEPTION;
  } catch (...) {
    std::cerr << "Fatal: unknown exception\n";
    return RC_EXCEPTION;
  }
}
