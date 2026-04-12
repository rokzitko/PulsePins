// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

class WebGuiService;
struct Verbosity;
struct WebGuiRuntimeConfig;

void run_ppwebgui_server(WebGuiService &service,
                         const WebGuiRuntimeConfig &config,
                         const Verbosity &verbosity);
