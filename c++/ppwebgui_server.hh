// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

class WebGuiService;
struct WebGuiAssets;
struct WebGuiHttpOptions;
struct WebGuiServerBinding;

void run_ppwebgui_server(WebGuiService &service,
                         const WebGuiAssets &assets,
                         const WebGuiServerBinding &binding,
                         const WebGuiHttpOptions &http_options);
