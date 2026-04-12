// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include "ppwebgui_frontend.hh"

class WebGuiService;

void run_ppwebgui_server(WebGuiService &service,
                         WebGuiAssets assets,
                         const WebGuiServerBinding &binding,
                         WebGuiHttpOptions http_options);
