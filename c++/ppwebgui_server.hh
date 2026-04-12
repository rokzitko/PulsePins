// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include "ppwebgui_assets.hh"
#include "ppwebgui_http.hh"

class WebGuiService;
struct WebGuiServerBinding;

void run_ppwebgui_server(WebGuiService &service,
                         WebGuiAssets assets,
                         const WebGuiServerBinding &binding,
                         WebGuiHttpOptions http_options);
