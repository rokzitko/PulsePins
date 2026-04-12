// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

namespace httplib {
class Server;
}

class WebGuiService;
struct WebGuiAssets;
struct WebGuiHttpOptions;

void register_ppwebgui_routes(httplib::Server &server,
                              WebGuiService &service,
                              const WebGuiHttpOptions &options,
                              const WebGuiAssets &assets);
