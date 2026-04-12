// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

class FPGA;
struct WebGuiRuntimeConfig;
struct Verbosity;

int run_ppwebgui(FPGA &fpga, const WebGuiRuntimeConfig &config, const Verbosity &verbosity);
