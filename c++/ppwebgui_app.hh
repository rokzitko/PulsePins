// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

class FPGA;
struct Verbosity;

#include "ppwebgui_config.hh"

int run_ppwebgui(FPGA &fpga, const WebGuiRuntimeConfig &config, const Verbosity &verbosity);
