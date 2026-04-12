// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

#include "ppwebgui_frontend.hh"

namespace httplib {
class Server;
}

class WebGuiService;

void register_ppwebgui_routes(httplib::Server &server,
                              WebGuiService &service,
                              WebGuiHttpOptions options,
                              WebGuiAssets assets);
