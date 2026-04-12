// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko
//
// Embedded host-side web GUI server for PulsePins.
//
// Note: this file is currently built with `-fno-inline` via the Makefile due to
// an optimization-sensitive crash observed on the board with the current ARM
// toolchain. Keep the workaround local to `ppwebgui` until the underlying
// miscompile/UB is understood better.

#include "host_runtime.hh"
#include "ppwebgui_app.hh"
#include "ppwebgui_bootstrap.hh"
#include "ppversion.hh"

int main(int argc, char *argv[]) {
  install_ppwebgui_fatal_signal_handlers();
  HostRuntime runtime(argc, argv, version);
  return run_ppwebgui(runtime);
}
