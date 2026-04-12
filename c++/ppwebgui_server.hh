// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include "ppwebgui_assets.hh"

class WebGuiService;
struct Verbosity;
struct WebGuiServerBinding;

void run_ppwebgui_server(WebGuiService &service,
                         WebGuiAssets assets,
                         const WebGuiServerBinding &binding,
                         const Verbosity &verbosity);
