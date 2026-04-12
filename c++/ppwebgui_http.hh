// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Rok Zitko

#pragma once

namespace httplib {
class Server;
}

class WebGuiService;
struct Verbosity;

void register_ppwebgui_routes(httplib::Server &server,
                              WebGuiService &service,
                              const Verbosity &verbosity,
                              const char *index_html,
                              const char *app_css,
                              const char *app_js);
